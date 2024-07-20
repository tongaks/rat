#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <thread>
#include <mutex>
#include <vector>
#include <wx/wx.h>
#include <wx/grid.h>

class Window : public wxFrame {
	int serverSock = 0;
	int clientCount = 0;
	struct sockaddr_in serverAddr;
	std::vector<int> clientSocks;

	wxTextCtrl *updatesField;
	wxTextCtrl *clientsField;
	wxTextCtrl *commandField;
	wxGrid *clientTable;
	wxButton *sendButton;
	wxButton *startButton;
	wxButton *closeButton;
	std::mutex clientMutex;

public:
	Window(const wxString &Title, const wxSize &Size);

	void StartServer(wxCommandEvent &event);
	void ButtonClose(wxCommandEvent &event);
	void OnQuit(wxCloseEvent& event);
	void CloseSocket();
	void SendCommand(wxCommandEvent &event);
	void HandleConnection();
	void AddClient(std::string addr, int id);
	void CheckClientCount();
	void WarningText(wxString msg);
	void HandleClient(int sock, std::string addr);
	wxString GetClientAddr(int sock);
};

class App : public wxApp {
public:
	virtual bool OnInit();
};

bool App::OnInit() {
	Window *window = new Window("Communication", wxDefaultSize);
	window->Show(true);
	return true;
}

Window::Window(const wxString &Title, const wxSize &Size) :
wxFrame(NULL, wxID_ANY, Title, wxDefaultPosition, Size) {
    Bind(wxEVT_CLOSE_WINDOW, &Window::OnQuit, this);

    this->SetBackgroundColour(wxColor(150, 150, 150));

    wxFont font(12, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

	 startButton = new wxButton(this, wxID_ANY, "Start");
    startButton->Bind(wxEVT_BUTTON, &Window::StartServer, this);

    closeButton = new wxButton(this, wxID_ANY, "Close");
    closeButton->Bind(wxEVT_BUTTON, &Window::ButtonClose, this);

    sendButton = new wxButton(this, wxID_ANY, "Send");
    sendButton->Enable(false);

    updatesField = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(400, 100), wxTE_READONLY | wxTE_MULTILINE);
    updatesField->SetFont(font);

    commandField = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(400, 50), wxTE_MULTILINE);
    commandField->SetFont(font);

    clientTable = new wxGrid(this, wxID_ANY, wxDefaultPosition, wxSize(200, 200));
    clientTable->CreateGrid(0, 2);
    clientTable->EnableEditing(false);
    clientTable->SetColLabelValue(0, "ID");
    clientTable->SetColLabelValue(1, "Client Address");
    // clientTable->AutoSizeColumns();
    clientTable->HideRowLabels();

	 int colWidth = clientTable->GetColSize(0);
    clientTable->SetColSize(1,  clientTable->GetColSize(1) + colWidth);
    clientTable->SetColSize(0,  40);
 
    wxBoxSizer *verticalSizer = new wxBoxSizer(wxVERTICAL);
    verticalSizer->SetBackgroundColour(wxColor:)
    verticalSizer->Add(updatesField, 1, wxEXPAND | wxALL, 10);
    verticalSizer->Add(commandField, 0, wxEXPAND | wxALL, 10);
    
    wxBoxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    buttonSizer->SetBackgroundColour(wxColor(23, 23, 23));
    buttonSizer->Add(sendButton, 0, wxALL, 10);
    buttonSizer->Add(closeButton, 0, wxALL, 10);
    buttonSizer->Add(startButton, 0, wxALL, 10);

    verticalSizer->Add(buttonSizer, 1, wxALIGN_BOTTOM);

    wxBoxSizer *horizontalSizer = new wxBoxSizer(wxHORIZONTAL);
    horizontalSizer->Add(verticalSizer, 0, wxALIGN_CENTER | wxRIGHT, 10);
    horizontalSizer->Add(clientTable, 0, wxALIGN_CENTER | wxRIGHT | wxTOP | wxBOTTOM, 10);


    SetSizerAndFit(horizontalSizer);
}

void Window::ButtonClose(wxCommandEvent &event) {
	CloseSocket();
}

void Window::StartServer(wxCommandEvent &event) {
	std::cout << "Server starting..." << std::endl;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(3455);
	serverAddr.sin_addr.s_addr = INADDR_ANY;

	serverSock = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSock < 0) {
		std::cout << "Error on creating socket" << std::endl;
		WarningText("Error creating socket");
		CloseSocket();
	}
	
	int opt = 1;
	if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		WarningText("Error reusing the address");
		CloseSocket();
	}

	if (bind(serverSock, (sockaddr*) &serverAddr, sizeof(serverAddr)) != 0) {
		std::cout << "Error on binding" << std::endl;
	   	WarningText("Error on bind");
	   	CloseSocket();
	}

	if (listen(serverSock, 1) < 0) {
		std::cout << "Error on binding" << std::endl;
	} 	WarningText("Listening for connections...");

	std::thread conHandlerObj([this](){this->HandleConnection();});
	conHandlerObj.detach();

	startButton->Enable(false);
}

void Window::HandleConnection() {
	while (true) {
		int clientSock = accept(serverSock, nullptr, nullptr);
		if (clientSock < 0) {
			WarningText("Failed to accept the client.");
			continue;
		}

		{
			std::lock_guard<std::mutex> lock(clientMutex);
			clientSocks.push_back(clientSock);			
		}

		clientCount++;

		std::string clientAddr = GetClientAddr(clientSock).ToStdString();
		std::cout << "A client is connected: " << clientAddr << std::endl;
		WarningText("Client connected: " + clientAddr);
		AddClient(clientAddr, clientCount);

		std::thread clientHandlerObj(&Window::HandleClient, this, clientSock, clientAddr);
		clientHandlerObj.detach();

		std::thread sendObj([this](){CheckClientCount();});
		sendObj.detach();
	}
}

void Window::HandleClient(int sock, std::string addr) {
	std::cout << "Handling the client's connection." << std::endl;
	std::string address = addr;
   bool isConnected = true;
   while (true) {
	   char buffer[1024];
	   int bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
	   if (bytesReceived < 0 && !isConnected) {
	      perror("Error receiving data from client");
	      WarningText("Error receiving data from client");
			break;
	   } else if (bytesReceived == 0 && isConnected) {
	   	isConnected = false;
	      WarningText("Client: " + addr + " disconnected.");
	      std::cout << "Client disconnected." << std::endl;
			break;
	   } else {
	       buffer[bytesReceived] = '\0';
	       std::cout << "Received from client: " << buffer << std::endl;
	   }   	
   }

   close(sock);
}

void Window::CheckClientCount() {
	while (true) {
		if (clientCount > 0) {
			sendButton->Enable(true);
		}		
	}
}

void Window::AddClient(std::string addr, int id) {
	int rowCount = clientTable->GetNumberRows();
	clientTable->AppendRows(1);
	clientTable->SetCellValue(0, 0, wxString::Format("%d", id));
	clientTable->SetCellValue(0, 1, addr);
}

wxString Window::GetClientAddr(int sock) {
	struct sockaddr_in clientInfo;
	socklen_t clientAddrLen = sizeof(clientInfo);
	char clientAddr[INET_ADDRSTRLEN];

	getpeername(sock, (sockaddr*)&clientInfo, &clientAddrLen);
	inet_ntop(AF_INET, &(clientInfo.sin_addr), clientAddr, sizeof(clientAddr));
	
	return (std::string) clientAddr;
}

void Window::WarningText(wxString msg) {
	updatesField->AppendText("[!] " + msg + "\n");
}

void Window::CloseSocket() {
	if (serverSock != -1) {
		close(serverSock);
		serverSock = -1;
	} exit(1);
}

void Window::OnQuit(wxCloseEvent& event) {
	CloseSocket();
	std::cout << "Server closed." << std::endl;
}

wxIMPLEMENT_APP(App);