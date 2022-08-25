
#include <iostream>
#include <fstream>
#include <random>
#include "httplib.h"
#include "picojson.h"

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

int main() {
	cout << "Set password: ";
	cin >> password;
	Server server;
	server.Get("/", [&](const Request & req, Response & res) {
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
	server.Post("/login/", [&](const Request & _req, Response & res) {
		value req;
		parse(req, _req.body);
		if (req.get<object>()["user"].is<string>() && req.get<object>()["password"].is<string>()) {
			Talk talk;
			talk.str = req.get<object>()["password"].get<string>();
			if (talk.str == password) {
				talk.user = req.get<object>()["user"].get<string>();
				random_device rd;
				string lp = to_string(rd());
				res.set_content(lp, "text/plain");
				talks.push_back({talk.user + " 加入了聊天室", "系统"});
				accepts[lp] = true;
				users[lp] = talk.user;
				cout << "Token " + lp + " join in.\n";
			} else {
				res.set_content("error", "text/plain");
			}
		} else {
			res.set_content("error", "text/plain");
		}
	});
	server.Get("/list/", [&](const Request & req, Response & res) {
		if (req.params.find("token") != req.params.end()) {
			if (accepts[req.params.find("token")->second]) {
				value json;
				json.set<picojson::array>(picojson::array());
				picojson::array &arr = json.get<picojson::array>();
				for (Talk &i : talks) {
					value json;
					json.set<object>(object());
					json.get<object>()["string"].set<string>(i.str);
					json.get<object>()["user"].set<string>(i.user);
					arr.push_back(json);
				}
				res.set_content(json.serialize(), "application/json");
				return;
			}
		}
		res.set_content("[]", "application/json");
	});
	server.Post("/send/", [&](const Request & _req, Response & res) {
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
	cout<<"Server has already opened!\n";
	server.listen("0.0.0.0", 80);
	return 0;
}
