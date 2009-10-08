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

#define MAX_CHANS 30
#define BUFFER_LEN 1025
#define MESSAGE_LEN 512

typedef struct ircbot {
	char *nick;
	char *server;
	char *port;
	char *chans[MAX_CHANS];
	int sock;
	struct addrinfo hints, *res;
	char buf[BUFFER_LEN];
	int num_chans;
} IRCBot;

int match(const char *pattern, char *text, regmatch_t *pmatch, int size);
int raw(IRCBot *bot, char *format, ...);
int nick(IRCBot *bot, char *nick);
int join(IRCBot *bot, char *channel);
int part(IRCBot *bot, char *channel, char *format, ...);
int privmsg(IRCBot *bot, char *recip, char *format, ...);
int parse(IRCBot *bot);

#endif
