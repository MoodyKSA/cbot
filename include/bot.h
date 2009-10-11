#ifndef BOT_H
#define BOT_H

#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <regex.h>
#include <stdlib.h>

#define MAX_CHANS 30	// Maximum number of channels the bot will allow itself to be in
#define BUFFER_LEN 1025	// 1kb + trailing \0 ... does this need to be so large? I have no clue
#define MESSAGE_LEN 512	// IRC specific
#define MAX_PORT_LEN 5	// I doubt this'll change

typedef struct ircbot {
	char *nick;
	char *server;
	char *port;
	char *chans[MAX_CHANS];
	int sock;
	struct addrinfo hints, *res;
	char *buf;
	int num_chans;
} IRCBot;

typedef struct ircuser {
	char *nick;
	char *user;
	char *host;
} IRCUser;

typedef struct ircmessage {
	IRCUser *sender;
	char *type;
	char *recip;
	char *text;
} IRCMessage;

int match(const char *pattern, char *text, regmatch_t *pmatch, int size);
int raw(IRCBot *bot, char *format, ...);
int nick(IRCBot *bot, char *nick);
int join(IRCBot *bot, char *channel);
int part(IRCBot *bot, char *channel, char *format, ...);
int privmsg(IRCBot *bot, char *recip, char *format, ...);
int parse(IRCBot *bot, char *msg);
int fetch_data_from_socket(IRCBot *bot, char *text);

#endif
