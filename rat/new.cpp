#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <thread>
#include <mutex>
#include <vector>
#include <wx/wx.h>
#include <wx/grid.h>
#include "functions.h"

class Window : public wxFrame {
private:
	wxTextCtrl *updatesField;
	wxTextCtrl *commandField;
	wxTextCtrl *selectedClientField;
	wxStaticText *currentClient;

	wxGrid *clientTable;

	wxButton *sendButton;
	wxButton *startButton;
	wxButton *closeButton;

	int selectedClient;
	int currentSock = 0;
	int serverSock = 0;
	int clientCount = 0;
	std::vector<int> clientSocks;
	std::mutex clientMutex;
	struct sockaddr_in serverAddr;

public:
	Window(const wxString &Title, const wxSize &Size);

	void ClientSelected(wxGridEvent& event);
	void SelectedClientHandler(wxGridEvent& event);
	void StartServer(wxCommandEvent &event);
	void ButtonClose(wxCommandEvent &event);
	void OnQuit(wxCloseEvent& event);
	void CloseSocket();
	void HandleConnection();
	void AddClient(std::string addr, int id);
	void CheckClientCount();
	bool SendCmd(wxString cmd);
	void SendCmdHandler(wxCommandEvent &event);
	void WarningText(std::string msg);
	void HandleClient(int sock, std::string addr);
	std::string GetClientAddr(int sock);
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
	this->SetBackgroundColour(wxColor(73, 73, 73));
  wxFont font(12, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

	wxPanel *txtFieldPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(300, 200));
	wxPanel *clientTablePanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(200, 200));
	wxPanel *buttonsPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(200, 50));

	updatesField = new wxTextCtrl(txtFieldPanel, wxID_ANY, "", wxDefaultPosition, wxSize(200, 100), wxTE_MULTILINE | wxTE_READONLY);
	commandField = new wxTextCtrl(txtFieldPanel, wxID_ANY, "", wxDefaultPosition, wxSize(200, 200), wxTE_MULTILINE);
	updatesField->SetFont(font);
	commandField->SetFont(font);

	startButton = new wxButton(buttonsPanel, wxID_ANY, "Start", wxDefaultPosition, wxDefaultSize);
	startButton->Bind(wxEVT_BUTTON, &Window::StartServer, this);

	closeButton = new wxButton(buttonsPanel, wxID_ANY, "Close", wxDefaultPosition, wxDefaultSize);
	closeButton->Bind(wxEVT_BUTTON, &Window::ButtonClose, this);

	sendButton = new wxButton(buttonsPanel, wxID_ANY, "Send", wxDefaultPosition, wxDefaultSize);
	sendButton->Enable(false);
	sendButton->Bind(wxEVT_BUTTON, &Window::SendCmdHandler, this);

	clientTable = new wxGrid(clientTablePanel, wxID_ANY, wxDefaultPosition, wxSize(200, 200));
	clientTable->CreateGrid(0, 2);
	clientTable->EnableEditing(false);
	clientTable->SetColLabelValue(0, "ID");
	clientTable->SetColLabelValue(1, "Client Address");
	clientTable->HideRowLabels();
	clientTable->Bind(wxEVT_GRID_SELECT_CELL, &Window::SelectedClientHandler, this);

	clientTable->AutoSizeColumn(1);
  int totalColSizer = clientTable->GetColSize(0) + clientTable->GetColSize(1);
  int gridSize = clientTable->GetSize().GetWidth(); 
  int remaining = abs(totalColSizer - gridSize);
  clientTable->SetColSize(1, remaining + totalColSizer);

	wxBoxSizer *txtFieldPanelSizer = new wxBoxSizer(wxVERTICAL);
	txtFieldPanelSizer->Add(updatesField, 2, wxALIGN_BOTTOM | wxALIGN_CENTER_HORIZONTAL | wxEXPAND | wxALL, 10);
	txtFieldPanelSizer->Add(commandField, 1, wxALIGN_CENTER_HORIZONTAL | wxEXPAND | wxALL, 10);
	txtFieldPanel->SetSizer(txtFieldPanelSizer);

	wxBoxSizer *clientTablePanelSizer = new wxBoxSizer(wxHORIZONTAL);
	clientTablePanelSizer->Add(clientTable, 1, wxALIGN_CENTER_HORIZONTAL | wxALL, 10);
	clientTablePanel->SetSizer(clientTablePanelSizer);

	wxBoxSizer *buttonsPanelSizer = new wxBoxSizer(wxHORIZONTAL);
	buttonsPanelSizer->AddStretchSpacer();
	buttonsPanelSizer->Add(startButton, 0, wxALIGN_BOTTOM | wxALL, 10);
	buttonsPanelSizer->Add(closeButton, 0, wxALIGN_BOTTOM | wxALL, 10);
	buttonsPanelSizer->Add(sendButton, 0, wxALIGN_BOTTOM | wxALL, 10);
	buttonsPanel->SetSizer(buttonsPanelSizer);

	wxBoxSizer *horizontalSizer = new wxBoxSizer(wxHORIZONTAL);
	horizontalSizer->Add(txtFieldPanel, 1, wxEXPAND);
	horizontalSizer->Add(clientTablePanel, 1, wxEXPAND);

	wxBoxSizer *verticalSizer = new wxBoxSizer(wxVERTICAL);
	verticalSizer->Add(horizontalSizer, 1, wxEXPAND);
	verticalSizer->Add(buttonsPanel, 0, wxEXPAND);

	SetSizerAndFit(verticalSizer);

	Bind(wxEVT_CLOSE_WINDOW, &Window::OnQuit, this);
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

		std::string clientAddr = GetClientAddr(clientSock);
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
  bool isConnected = true;
  while (isConnected) {
	  char buffer[1024];
	  int bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);
	  if (bytesReceived > 0) {
      buffer[bytesReceived] = '\0'; // Null-terminate the received data
      std::cout << "Received from client: " << buffer << std::endl;
	  } else if (bytesReceived == 0) {
      std::cout << "Client disconnected." << std::endl;
      WarningText("Client: " + addr + " disconnected.");
      isConnected = false;
	  } else {
      perror("Error receiving data from client");
      WarningText("Error receiving data from client");
      isConnected = false;
	  }
  }

  close(sock);
}

void Window::SelectedClientHandler(wxGridEvent &event) {
  int selectedRow = event.GetRow();

  std::string idStr = clientTable->GetCellValue(selectedRow, 0).ToStdString();
  if (!idStr.empty()) {
    try {
      selectedClient = std::stoi(idStr);
      if (selectedClient >= 0 && selectedClient < clientSocks.size()) {
        currentSock = clientSocks[selectedClient];
        std::cout << "id: " << selectedClient << std::endl;
        WarningText("Client selected: " + std::to_string(selectedClient));
	    } else {
	       WarningText("Selected client index is out of bounds.");
	    }
	  } catch (const std::exception& e) {
       WarningText("Error parsing client ID: " + std::string(e.what()));
    }
  }

    event.Skip();
}

bool Window::SendCmd(wxString cmd) {
  if (selectedClient < 0 || selectedClient >= clientSocks.size()) {
    WarningText("Invalid client index.");
    return false;
  }

  std::string cmdStr = cmd.ToStdString(); // Convert wxString to std::string
  int res = send(clientSocks[selectedClient - 1], cmdStr.c_str(), cmdStr.size(), 0);

  if (res >= 0) {
      return true;
  }

  WarningText("Failed to send command.");
  std::cout << "res: " << res << "\n";
  std::cout << "errno: " << errno << "\n";
  perror("Error");
  return false;
}


void Window::SendCmdHandler(wxCommandEvent &event) {
	std::string cmd = commandField->GetValue().ToStdString();
	if (SendCmd(cmd)) WarningText("Command was sent.");
}

void Window::CheckClientCount() {
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(1)); // Avoid busy-waiting
    if (clientCount > 0) {
      sendButton->Enable(true);
      break;
    }
  }
}


void Window::AddClient(std::string addr, int id) {
	int rowCount = clientTable->GetNumberRows();
	clientTable->AppendRows(1);
	clientTable->SetCellValue(0, 0, wxString::Format("%d", id));
	clientTable->SetCellValue(0, 1, addr);
}

std::string Window::GetClientAddr(int sock) {
	struct sockaddr_in clientInfo;
	socklen_t clientAddrLen = sizeof(clientInfo);
	char clientAddr[INET_ADDRSTRLEN];

	getpeername(sock, (sockaddr*)&clientInfo, &clientAddrLen);
	inet_ntop(AF_INET, &(clientInfo.sin_addr), clientAddr, sizeof(clientAddr));
	
	return (std::string) clientAddr;
}

void Window::WarningText(std::string msg) {
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