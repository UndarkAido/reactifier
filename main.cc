#include <iostream>
#include <fstream>

#include <boost/asio.hpp>

#include <discordpp/bot.hh>
#include <discordpp/plugin-overload.hh>
#include <discordpp/rest-beast.hh>
#include <discordpp/websocket-beast.hh>

namespace asio = boost::asio;
using json = nlohmann::json;
namespace dpp = discordpp;
using DppBot = dpp::PluginOverload<dpp::WebsocketBeast<dpp::RestBeast<dpp::Bot> > >;

std::istream &safeGetline(std::istream &is, std::string &t);

void filter(std::string &target, const std::string &pattern);


int main(){
	std::cout << "Starting bot...\n\n";

	/*/
	 * Read token from token file.
	 * Tokens are required to communicate with Discord, and hardcoding tokens is a bad idea.
	 * If your bot is open source, make sure it's ignore by git in your .gitignore file.
	/*/
	std::string token;
	{
		std::ifstream tokenFile("token.dat");
		if(!tokenFile){
			std::cerr << "CRITICAL: "
			          << "There is no valid way for Echo to obtain a token! "
			          << "Copy the example `token.eg.dat` as `token.dat` to make one.\n";
			exit(1);
		}
		safeGetline(tokenFile, token);
		tokenFile.close();
	}

	// Create Bot object
	auto bot = std::make_shared<DppBot>();
	// Don't complain about unhandled events
	bot->debugUnhandled = false;

	/*/
	 * Create handler for the READY payload, this may be handled by the bot in the future.
	 * The `self` object contains all information about the 'bot' user.
	/*/
	json self;
	bot->handlers.insert(
			{
					"READY",
					[&bot, &self](json data){
						self = data["user"];
					}
			}
	);

	// Create handler for the MESSAGE_CREATE payload, this receives all messages sent that the bot can see.
	bot->handlers.insert(
			{
					"MESSAGE_REACTION_ADD",
					[&bot, &self](const json &msg){
						std::ostringstream send;
						send << msg["member"]["nick"].get<std::string>() << " reacted ";
						if(msg["emoji"]["id"].is_null()){
							send << msg["emoji"]["name"].get<std::string>();
						}else{
							send << "<:" << msg["emoji"]["name"].get<std::string>() << ":"
							     << msg["emoji"]["id"].get<std::string>() << ">";
						}
						std::string content = send.str();
						bot->call(
								"GET",
								"/channels/" + msg["channel_id"].get<std::string>() + "/messages/" +
								msg["message_id"].get<std::string>(),
								[&bot, &self, content](const json &msg){
									bot->call(
											"POST",
											"/users/@me/channels",
											json({{"recipient_id", msg["author"]["id"]}}),
											[&bot, &self, content](const json &msg){
												bot->call(
														"POST",
														"/channels/" + msg["id"].get<std::string>() + "/messages",
														json({{"content", content}})
												);
											}
									);
								}
						);
					}
			}
	);

	// Create Asio context, this handles async stuff.
	auto aioc = std::make_shared<asio::io_context>();

	// Set the bot up
	bot->initBot(6, token, aioc);

	// Run the bot!
	bot->run();

	return 0;
}

/*/
 * Source: https://stackoverflow.com/a/6089413/1526048
/*/
std::istream &safeGetline(std::istream &is, std::string &t){
	t.clear();

	// The characters in the stream are read one-by-one using a std::streambuf.
	// That is faster than reading them one-by-one using the std::istream.
	// Code that uses streambuf this way must be guarded by a sentry object.
	// The sentry object performs various tasks,
	// such as thread synchronization and updating the stream state.

	std::istream::sentry se(is, true);
	std::streambuf *sb = is.rdbuf();

	for(;;){
		int c = sb->sbumpc();
		switch(c){
			case '\n':
				return is;
			case '\r':
				if(sb->sgetc() == '\n'){
					sb->sbumpc();
				}
				return is;
			case std::streambuf::traits_type::eof():
				// Also handle the case when the last line has no line ending
				if(t.empty()){
					is.setstate(std::ios::eofbit);
				}
				return is;
			default:
				t += (char)c;
		}
	}
}

void filter(std::string &target, const std::string &pattern){
	while(target.find(pattern) != std::string::npos){
		target = target.substr(0, target.find(pattern)) +
		         target.substr(target.find(pattern) + (pattern).size());
	}
}