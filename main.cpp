#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <random>
#include <string>

#include "httplib.h"  // from https://github.com/dropbox/json11, MIT license
#include "json11.hpp" // from https://github.com/yhirose/cpp-httplib, MIT license

class request_error : public std::exception {
    std::string err;
    int status_;

  public:
    request_error(int status, const std::string &err) {
        this->status_ = status;
        this->err = err;
    }
    virtual const char *what() {
        return err.c_str();
    }
    int status() {
        return status_;
    }
};
class auto_mutex {
    std::mutex &mutex_;

  public:
    auto_mutex(std::mutex &mutex__) : mutex_(mutex__) {
        mutex_.lock();
    }
    ~auto_mutex() {
        mutex_.unlock();
    }
    void lock() {
        mutex_.lock();
    }
    void unlock() {
        mutex_.unlock();
    }
};
std::string readfile(const std::string &filename) {
    std::ifstream fin(filename);
    return std::string((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
}
std::random_device rd;

json11::Json errorJSON(const std::string &error) {
    return json11::Json::object{{"code", -1}, {"msg", error}, {"data", nullptr}};
}
json11::Json errorJSON(int error) {
    return json11::Json::object{{"code", -1}, {"msg", error}, {"data", nullptr}};
}
json11::Json successJSON(const json11::Json::object &obj) {
    return json11::Json::object{{"code", 0}, {"msg", "successful"}, {"data", obj}};
}
json11::Json successJSONArray(const json11::Json::array &obj) {
    return json11::Json::object{{"code", 0}, {"msg", "successful"}, {"data", obj}};
}
json11::Json parseJSON(const std::string &str) {
    std::string err;
    auto json = json11::Json::parse(str, err);
    if (!err.empty()) throw(request_error(400, err));
    return json;
}
template <typename T> T getJSON(const json11::Json &json, const std::string &key);
template <> double getJSON(const json11::Json &json, const std::string &key) {
    if (!json[key].is_number()) throw request_error(400, "type error");
    return json[key].number_value();
}
template <> std::string getJSON(const json11::Json &json, const std::string &key) {
    if (!json[key].is_string()) throw request_error(400, "type error");
    return json[key].string_value();
}
json11::Json newJSON(const json11::Json::object &obj) {
    return json11::Json(obj);
}

std::string password;
json11::Json::object users;
std::mutex users_mutex;
json11::Json::array messages;
std::mutex messages_mutex;

std::string verifyToken(const httplib::Request &req) {
    if (!req.has_header("token")) {
        throw request_error(403, "未提供令牌");
    }
    auto token_ = req.get_header_value("token");
    if (users.find(token_) == users.end()) {
        throw request_error(403, "令牌无效");
    }
    return token_;
}

int main() {
    // 编码设置
    std::setlocale(LC_ALL, "chs.utf8");
    // 设置邀请码
    std::cout << "Set password: ";
    std::cin >> password;
    // 配置服务端
    httplib::Server server;
    server.set_payload_max_length(8192);
    server.set_error_handler([](const auto &req, auto &res) {
        // 错误
        if (res.body.empty()) {
            res.set_content(errorJSON(std::to_string(res.status)).dump(), "application/json");
        }
    });
    server.set_exception_handler([](const auto &req, auto &res, auto ep) {
        // 异常
        try {
            rethrow_exception(ep);
        } catch (request_error &e) {
            res.status = e.status();
            res.set_content(errorJSON(e.what()).dump(), "application/json");
        }
    });
    server.Get("/", [](const auto &req, auto &res) {
        // 登录页面
        res.set_content(readfile("pages/login.html"), "text/html");
    });
    server.Post("/login/", [](const auto &req, auto &res) {
        // 登录接口
        auto json = parseJSON(req.body);
        auto username_ = getJSON<std::string>(json, "username");
        auto password_ = getJSON<std::string>(json, "password");
        if (password_ != password) {
            throw request_error(401, "邀请码错误");
        }
        if (username_ == "") {
            throw request_error(409, "用户名为空");
        }
        auto_mutex mutex_(users_mutex);
        for (const auto &i : users) {
            if (i.second["username"].string_value() == username_) {
                throw request_error(409, "用户名重复");
            }
        }
        auto user = newJSON({{"username", username_}});
        auto token = std::to_string(rd());
        if (users.find(token) != users.end()) {
            throw request_error(500, "系统随机数重复，请重试");
        }
        users[token] = user;
        mutex_.unlock();
        std::clog << "New user \"" << username_ << "\" login.\n";
        res.status = 200;
        res.set_content(successJSON({{"token", token}}).dump(), "application/json");
    });
    server.Get("/room/", [](const auto &req, auto &res) {
        // 聊天页面
        res.set_content(readfile("pages/room.html"), "text/html");
    });
    server.Post("/post/", [](const auto &req, auto &res) {
        // 发送信息接口
        auto token = verifyToken(req);
        auto json = parseJSON(req.body);
        auto body = getJSON<std::string>(json, "body");
        if (body.empty()) {
            throw request_error(409, "消息内容为空");
        }
        auto msg = newJSON({
            {"id", (double)messages.size()},
            {"user", users[token]["username"].string_value()},
            {"body", body},
        });
        auto_mutex mutex_(messages_mutex);
        messages.push_back(msg);
        mutex_.unlock();
        res.set_content(successJSON({}).dump(), "text/html");
    });
    server.Post("/get/", [](const auto &req, auto &res) {
        // 获取信息接口
        auto token = verifyToken(req);
        auto json = parseJSON(req.body);
        int id = getJSON<double>(json, "id");
        while (id >= messages.size()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
        res.set_content(successJSON(messages[id].object_items()).dump(), "text/html");
    });
    // 开启服务端
    std::clog << "Server started on http://0.0.0.0:80/.\n";
    server.listen("0.0.0.0", 80);
    return 0;
}