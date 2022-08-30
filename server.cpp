
#include "httplib.h"
#include "picojson.h"
#include <fstream>
#include <iostream>
#include <random>

using namespace std;
using namespace httplib;
using namespace picojson;

struct Talk {
    string str;
    string user;
};
vector<Talk> talks;

string password;
map<string, bool> accepts;
map<string, string> users;
map<string, size_t> poses;

Server server;

void start() {
    cout << "Set password: ";
    cin >> password;
    server.set_payload_max_length(4096);
    server.set_error_handler([](const Request &req, Response &res) {
        res.status = 200;
        res.set_content("error", "text/plain");
    });
    server.set_exception_handler([](const Request &req, Response &res, exception_ptr ep) {
        try {
            rethrow_exception(ep);
        } catch (exception &e) {
            cout << "Server crash! \n";
            cout << "Type: " << e.what() << " (in server thread)\n";
            exit(1);
        } catch (...) {
            cout << "Server crash! \n";
            cout << "Type: unknown exception (in server thread)\n";
            exit(1);
        }
    });
    server.Get("/", [&](const Request &req, Response &res) {
        if (req.params.find("token") != req.params.end()) {
            if (accepts[req.params.find("token")->second]) {
                ifstream fin("index.html");
                string str((istreambuf_iterator<char>(fin)), istreambuf_iterator<char>());
                res.set_content(str, "text/html");
                return;
            }
        }
        ifstream fin("login.html");
        string str((istreambuf_iterator<char>(fin)), istreambuf_iterator<char>());
        res.set_content(str, "text/html");
    });
    server.Post("/login/", [&](const Request &_req, Response &res) {
        value req;
        parse(req, _req.body);
        if (req.get<object>()["user"].is<string>() && req.get<object>()["password"].is<string>()) {
            Talk talk;
            talk.str = req.get<object>()["password"].get<string>();
            if (talk.str == password) {
                if (talk.user != "系统") {
                    talk.user = req.get<object>()["user"].get<string>();
                    random_device rd;
                    string lp = to_string(rd()) + to_string(rd());
                    res.set_content(lp, "text/plain");
                    talks.push_back({talk.user + " 加入了聊天室", "系统"});
                    accepts[lp] = true;
                    users[lp] = talk.user;
                    poses[lp] = 0;
                    cout << "Token " + lp + " join in.\n";
                }
            } else {
                res.set_content("error", "text/plain");
            }
        } else {
            res.set_content("error", "text/plain");
        }
    });
    server.Get("/new/", [&](const Request &req, Response &res) {
        if (req.params.find("token") != req.params.end()) {
            string token = req.params.find("token")->second;
            if (accepts[token]) {
                value json;
                json.set<picojson::array>(picojson::array());
                picojson::array &arr = json.get<picojson::array>();
                for (; poses[token] < talks.size(); poses[token]++) {
                    Talk &i = talks[poses[token]];
                    value json;
                    json.set<object>(object());
                    json.get<object>()["string"].set<string>(i.str);
                    json.get<object>()["user"].set<string>(i.user);
                    const double &pp = poses[token];
                    json.get<object>()["id"].set<double>(pp);
                    arr.push_back(json);
                }
                res.set_content(json.serialize(), "application/json");
                return;
            }
        }
        res.set_content("[]", "application/json");
    });
    server.Get("/list/", [&](const Request &req, Response &res) {
        if (req.params.find("token") != req.params.end()) {
            string token = req.params.find("token")->second;
            if (accepts[token]) {
                value json;
                json.set<picojson::array>(picojson::array());
                picojson::array &arr = json.get<picojson::array>();
                for (size_t pos = 0; pos < talks.size(); pos++) {
                    Talk &i = talks[pos];
                    value json;
                    json.set<object>(object());
                    json.get<object>()["string"].set<string>(i.str);
                    json.get<object>()["user"].set<string>(i.user);
                    const double &pp = pos;
                    json.get<object>()["id"].set<double>(pp);
                    arr.push_back(json);
                }
                res.set_content(json.serialize(), "application/json");
                return;
            }
        }
        res.set_content("[]", "application/json");
    });
    server.Post("/send/", [&](const Request &_req, Response &res) {
        value req;
        parse(req, _req.body);
        if (req.is<object>()) {
            if (req.get<object>()["string"].is<string>() && req.get<object>()["token"].is<string>()) {
                if (accepts[req.get<object>()["token"].get<string>()]) {
                    Talk talk;
                    talk.str = req.get<object>()["string"].get<string>();
                    talk.user = users[req.get<object>()["token"].get<string>()];
                    talks.push_back(talk);
                    res.set_content("success", "text/plain");
                    return;
                }
            }
        }
        res.set_content("error", "text/plain");
    });
    cout << "Server has already opened!\n";
    thread command_thread([&]() {
        try {
            while (1) {
                string cmd;
                cin >> cmd;
                if (cmd == "crash") {
                    throw 0;
                } else if (cmd == "user") {
                    for (auto i : users) {
                        cout << i.first << " : " << i.second << endl;
                    }
                } else if (cmd == "kick") {
                    string token, why;
                    cout << "User token: ";
                    cin >> token;
                    cout << "Reason: ";
                    cin >> why;
                    if (accepts.find(token) != accepts.end()) {
                        accepts.erase(accepts.find(token));
                        Talk talk;
                        talk.str = users[token] + " 已被系统踢出！原因：" + why;
                        talk.user = "系统";
                        talks.push_back(talk);
                    } else {
                        cout << "Unknown user.\n";
                    }
                } else if (cmd == "list") {
                    cout << "Now server has " << talks.size() << " talks.\n";
                } else if (cmd == "delete") {
                    size_t id;
                    string why;
                    cout << "Talk id: ";
                    cin >> id;
                    cout << "Reason: ";
                    cin >> why;
                    if (id >= 0 && id < talks.size()) {
                        talks[id].str = "此消息已被系统删除！原因：" + why;
                    } else {
                        cout << "Unknown talk.\n";
                    }
                } else if (cmd == "help") {
                    cout << "crash.....crash this server\n";
                    cout << "user......list users\n";
                    cout << "kick......kick user\n";
                    cout << "list......list talks\n";
                    cout << "delete....delete talk\n";
                } else {
                    cout << "Unknown command. Type 'help' to find help.\n";
                }
            }
        } catch (exception &e) {
            cout << "Server crash! \n";
            cout << "Type: " << e.what() << " (in command thread)\n";
            exit(1);
        } catch (...) {
            cout << "Server crash! \n";
            cout << "Type: unknown exception (in command thread)\n";
            exit(1);
        }
    });
    server.listen("0.0.0.0", 80);
}

int main() {
    try {
        start();
    } catch (exception &e) {
        cout << "Server crash! \n";
        cout << "Type: " << e.what() << " (in main thread)\n";
        exit(1);
    } catch (...) {
        cout << "Server crash! \n";
        cout << "Type: unknown exception (in main thread)\n";
        exit(1);
    }
    return 0;
}
