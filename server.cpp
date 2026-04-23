#include <iostream>
#include <string>
#include <sstream>
#include <stdexcept>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

using namespace std;

string trim(const string& s) {
    size_t start = s.find_first_not_of(" \n\r\t");
    size_t end = s.find_last_not_of(" \n\r\t");
    if (start == string::npos || end == string::npos) return "";
    return s.substr(start, end - start + 1);
}

string escapeJson(const string& s) {
    string out;
    for (char c : s) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out;
}

string decodeURL(const string& str) {
    string result;
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '%' && i + 2 < str.length()) {
            string hex = str.substr(i + 1, 2);
            char ch = static_cast<char>(strtol(hex.c_str(), nullptr, 16));
            result += ch;
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

string getCurrentTimeString() {
    time_t now = time(nullptr);
    tm* local = localtime(&now);
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%d-%m-%Y %H:%M:%S", local);
    return string(buffer);
}

string extractJsonValue(const string& body, const string& key) {
    string searchKey = "\"" + key + "\"";
    size_t keyPos = body.find(searchKey);
    if (keyPos == string::npos) return "";

    size_t colonPos = body.find(':', keyPos);
    if (colonPos == string::npos) return "";

    size_t firstQuote = body.find('"', colonPos + 1);
    if (firstQuote == string::npos) return "";

    size_t secondQuote = firstQuote + 1;
    while (true) {
        secondQuote = body.find('"', secondQuote);
        if (secondQuote == string::npos) return "";
        if (body[secondQuote - 1] != '\\') break;
        secondQuote++;
    }

    return body.substr(firstQuote + 1, secondQuote - firstQuote - 1);
}

string getRequestPath(const string& request) {
    size_t firstSpace = request.find(' ');
    if (firstSpace == string::npos) return "";
    size_t secondSpace = request.find(' ', firstSpace + 1);
    if (secondSpace == string::npos) return "";
    return request.substr(firstSpace + 1, secondSpace - firstSpace - 1);
}

string getQueryParam(const string& path, const string& key) {
    size_t qPos = path.find('?');
    if (qPos == string::npos) return "";

    string query = path.substr(qPos + 1);
    string searchKey = key + "=";

    size_t keyPos = query.find(searchKey);
    if (keyPos == string::npos) return "";

    size_t valueStart = keyPos + searchKey.length();
    size_t valueEnd = query.find('&', valueStart);

    string value;
    if (valueEnd == string::npos) {
        value = query.substr(valueStart);
    } else {
        value = query.substr(valueStart, valueEnd - valueStart);
    }

    return decodeURL(value);
}

class Message {
protected:
    string sender;
    string receiver;
    string content;
    string timestamp;

public:
    Message(string s, string r, string c)
        : sender(s), receiver(r), content(c), timestamp(getCurrentTimeString()) {}

    virtual ~Message() {}

    virtual string getType() const = 0;
    virtual string getDisplayContent() const = 0;

    string getSender() const { return sender; }
    string getReceiver() const { return receiver; }

    virtual string toJson() const {
        stringstream ss;
        ss << "{"
           << "\"sender\":\"" << escapeJson(sender) << "\","
           << "\"receiver\":\"" << escapeJson(receiver) << "\","
           << "\"type\":\"" << escapeJson(getType()) << "\","
           << "\"content\":\"" << escapeJson(getDisplayContent()) << "\","
           << "\"time\":\"" << escapeJson(timestamp) << "\""
           << "}";
        return ss.str();
    }
};

class TextMessage : public Message {
public:
    TextMessage(string s, string r, string c) : Message(s, r, c) {}
    string getType() const override { return "text"; }
    string getDisplayContent() const override { return content; }
};

class ImageMessage : public Message {
public:
    ImageMessage(string s, string r, string c) : Message(s, r, c) {}
    string getType() const override { return "image"; }
    string getDisplayContent() const override { return "Image URL: " + content; }
};

class VoiceMessage : public Message {
public:
    VoiceMessage(string s, string r, string c) : Message(s, r, c) {}
    string getType() const override { return "voice"; }
    string getDisplayContent() const override { return "Voice Note: " + content; }
};

class Node {
public:
    Message* data;
    Node* next;

    Node(Message* msg) : data(msg), next(nullptr) {}
    ~Node() { delete data; }
};

class MessageLinkedList {
private:
    Node* head;
    Node* tail;

public:
    MessageLinkedList() : head(nullptr), tail(nullptr) {}

    ~MessageLinkedList() {
        Node* current = head;
        while (current != nullptr) {
            Node* temp = current;
            current = current->next;
            delete temp;
        }
    }

    void insert(Message* msg) {
        Node* newNode = new Node(msg);
        if (head == nullptr) {
            head = tail = newNode;
        } else {
            tail->next = newNode;
            tail = newNode;
        }
    }

    string toJsonArrayForUsers(const string& user1, const string& user2) const {
        stringstream ss;
        ss << "[";
        bool first = true;

        Node* current = head;
        while (current != nullptr) {
            string s = current->data->getSender();
            string r = current->data->getReceiver();

            bool match = (s == user1 && r == user2) || (s == user2 && r == user1);

            if (match) {
                if (!first) ss << ",";
                ss << current->data->toJson();
                first = false;
            }

            current = current->next;
        }

        ss << "]";
        return ss.str();
    }
};

class ChatManager {
private:
    MessageLinkedList messages;

    void validateInput(const string& sender, const string& receiver, const string& type, const string& content) {
        if (trim(sender).empty()) throw invalid_argument("Sender name cannot be empty.");
        if (trim(receiver).empty()) throw invalid_argument("Receiver name cannot be empty.");
        if (trim(type).empty()) throw invalid_argument("Message type cannot be empty.");
        if (trim(content).empty()) throw invalid_argument("Message content cannot be empty.");
        if (sender.length() > 50 || receiver.length() > 50) throw invalid_argument("Name too long.");
        if (content.length() > 500) throw invalid_argument("Message too long.");
    }

public:
    void addMessage(const string& sender, const string& receiver, const string& type, const string& content) {
        validateInput(sender, receiver, type, content);

        Message* msg = nullptr;

        if (type == "text") msg = new TextMessage(sender, receiver, content);
        else if (type == "image") msg = new ImageMessage(sender, receiver, content);
        else if (type == "voice") msg = new VoiceMessage(sender, receiver, content);
        else throw invalid_argument("Invalid message type.");

        messages.insert(msg);
    }

    string getPrivateMessagesJson(const string& user1, const string& user2) const {
        return messages.toJsonArrayForUsers(user1, user2);
    }
};

ChatManager chatManager;

string makeHttpResponse(const string& body, const string& status = "200 OK") {
    stringstream ss;
    ss << "HTTP/1.1 " << status << "\r\n"
       << "Content-Type: application/json\r\n"
       << "Access-Control-Allow-Origin: *\r\n"
       << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
       << "Access-Control-Allow-Headers: Content-Type\r\n"
       << "Content-Length: " << body.size() << "\r\n"
       << "Connection: close\r\n\r\n"
       << body;
    return ss.str();
}

string makeOptionsResponse() {
    return "HTTP/1.1 204 No Content\r\n"
           "Access-Control-Allow-Origin: *\r\n"
           "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
           "Access-Control-Allow-Headers: Content-Type\r\n"
           "Connection: close\r\n\r\n";
}

string makeErrorJson(const string& message) {
    return "{\"status\":\"error\",\"message\":\"" + escapeJson(message) + "\"}";
}

string makeSuccessJson(const string& message, const string& messagesJson) {
    return "{\"status\":\"success\",\"message\":\"" + escapeJson(message) + "\",\"messages\":" + messagesJson + "}";
}

string handleRequest(const string& request) {
    if (request.find("OPTIONS ") == 0) {
        return makeOptionsResponse();
    }

    string path = getRequestPath(request);

    if (path == "/") {
        return makeHttpResponse("{\"status\":\"success\",\"message\":\"C++ WhatsApp backend is running\"}");
    }

    if (path.find("/api/messages") == 0 && request.find("GET ") == 0) {
        string sender = getQueryParam(path, "sender");
        string receiver = getQueryParam(path, "receiver");

        if (sender.empty() || receiver.empty()) {
            return makeHttpResponse(makeErrorJson("sender and receiver are required."), "400 Bad Request");
        }

        string body = makeSuccessJson(
            "Private messages fetched successfully.",
            chatManager.getPrivateMessagesJson(sender, receiver)
        );
        return makeHttpResponse(body);
    }

    if (request.find("POST /api/messages ") == 0) {
        size_t bodyPos = request.find("\r\n\r\n");
        if (bodyPos == string::npos) {
            return makeHttpResponse(makeErrorJson("Invalid request body."), "400 Bad Request");
        }

        string body = request.substr(bodyPos + 4);

        try {
            string sender = extractJsonValue(body, "sender");
            string receiver = extractJsonValue(body, "receiver");
            string type = extractJsonValue(body, "type");
            string content = extractJsonValue(body, "content");

            chatManager.addMessage(sender, receiver, type, content);

            string responseBody = makeSuccessJson(
                "Message added successfully.",
                chatManager.getPrivateMessagesJson(sender, receiver)
            );

            return makeHttpResponse(responseBody);
        } catch (const exception& e) {
            return makeHttpResponse(makeErrorJson(e.what()), "400 Bad Request");
        }
    }

    return makeHttpResponse(makeErrorJson("Route not found."), "404 Not Found");
}

int main() {
    int port = 10000;
    const char* envPort = getenv("PORT");
    if (envPort != nullptr) port = atoi(envPort);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        cerr << "Socket creation failed\n";
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        cerr << "Bind failed\n";
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        cerr << "Listen failed\n";
        close(server_fd);
        return 1;
    }

    cout << "Server running on port " << port << endl;

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) continue;

        char buffer[16384];
        int bytesRead = read(client_socket, buffer, sizeof(buffer) - 1);

        if (bytesRead <= 0) {
            close(client_socket);
            continue;
        }

        buffer[bytesRead] = '\0';
        string request(buffer);

        string response = handleRequest(request);
        send(client_socket, response.c_str(), response.size(), 0);

        close(client_socket);
    }

    close(server_fd);
    return 0;
}
