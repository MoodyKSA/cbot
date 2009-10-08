#include <bot.h>

int init_socket(IRCBot *bot, char *nick, char *server, char *port)
{
	bot->nick = nick;
	bot->server = server;
	bot->port = port;
	bot->num_chans = 0;
	memset( &bot->hints, 0, sizeof bot->hints);
	bot->hints.ai_family = AF_UNSPEC;
	bot->hints.ai_socktype = SOCK_STREAM;
	switch(getaddrinfo( bot->server, bot->port, &bot->hints, &bot->res))
	{
		case -2:
			printf("Error: Name or Service not known\n");
			return -2;
		case -3:
			printf("Error: Temporary failure in name resolution\n");
			return -3;
		case -4:
			printf("Error: Failure in name resolution\n");
			return -4;
		case -11:
			printf("System returned error %i\n", errno);
			return errno;
	}

	// Socket!
	bot->sock = socket(bot->res->ai_family, bot->res->ai_socktype, bot->res->ai_protocol);
	printf("Attempting to connect to %s:%s\n", bot->server, bot->port);
	if(connect(bot->sock, bot->res->ai_addr, bot->res->ai_addrlen) != -1)  {
		// It worked
		printf("Connected!\n");
	} else {
		// It Failed
		printf("Error: %i\n", errno);
		return errno;
	}
	return 0;
}

int init_connection(IRCBot *bot)
{
	raw(bot, "USER %s 0 0 :%s", bot->nick, bot->nick);
	nick(bot, bot->nick);
	return 1;
}

int match(const char *pattern, char *text, regmatch_t *pmatch, int size)
{
	int rc = -1;
	regex_t regexp;
	if (0 != (rc = regcomp(&regexp, pattern, REG_EXTENDED))) {
		fprintf(stderr, "regcomp() failed, returning nonzero (%d)\n", rc);
	} else {
		if (0 != (rc = regexec(&regexp, text, (size_t)size, pmatch, 0))) {
			if ( rc == 1 )
				rc = -1;
		} else {
			rc = 1;
		}
	}
	regfree(&regexp);
	return rc;
}

int raw(IRCBot *bot, char *format, ...)
{
	char sendbuf[BUFFER_LEN];
	va_list args;
	va_start(args, format);
	memset(&sendbuf, 0, strlen(sendbuf));
	
	vsnprintf(sendbuf, BUFFER_LEN-1, format, args); //buf contains the formatted string
	strcpy( (sendbuf+strlen(sendbuf)), "\r\n");
	
	int len = strlen(sendbuf);
	int sent = send(bot->sock, sendbuf, len, 0);
	printf("<< %s", sendbuf);
	return sent;
}

int nick(IRCBot *bot, char *nick)
{
	bot->nick = nick;
	return raw(bot, "NICK %s", nick);
}

int join(IRCBot *bot, char *channel)
{
	int sent;
	if ( bot->num_chans < MAX_CHANS ) {
		sent = raw(bot, "JOIN %s", channel);
		//bot.chans[bot.num_chans] = channel;
		//bot.num_chans++;
		return sent;
	} else {
		return -1;
	}
}

int part(IRCBot *bot, char *channel, char *format, ...)
{
	// TODO: Implement part()
	return 0;
}

int privmsg(IRCBot *bot, char *recip, char *format, ...)
{
	char sendbuf[BUFFER_LEN];
	va_list args;
	va_start(args, format);
	
	memset(&sendbuf, 0, strlen(sendbuf));
	
	vsprintf(sendbuf, format, args); //buf contains the formatted string
	return raw(bot, "PRIVMSG %s :%s", recip, sendbuf);
}

int parse(IRCBot *bot)
{
	char msg[BUFFER_LEN];
	regmatch_t pmatch[4];
	
	printf(">> %s", bot->buf);

	strcpy(msg, bot->buf);
	
	if ( match("^PING", msg, pmatch, 1) == 1 ) {
		raw(bot, "PONG %s\r\n", &(msg[5]));
	}

	if ( match("([^:][^@]+[@][^ ]+)", msg, pmatch, 1) == 1) {
		// Yes, i am absolutely clueless as to how this works :D
		privmsg(bot, "#baddog", "%#x - %#x = %#x (%#x) in (%#x) \"%s\".", pmatch[0].rm_eo, pmatch[0].rm_so, (pmatch[0].rm_eo-pmatch[0].rm_so), ((size_t)pmatch[0].rm_eo-pmatch[0].rm_so)-(size_t)&msg, &msg, msg);
	}
	return 0;
}

int fetch_data_from_socket(IRCBot *bot)
{
	unsigned int i;
	int break_loop = 0;
	memset(&bot->buf, 0, BUFFER_LEN);

	while ( recv(bot->sock, &(bot->buf[strlen(bot->buf)]), 1, 0) && (strlen(bot->buf)<=MESSAGE_LEN) ) {
		for ( i = 0; i < MESSAGE_LEN; i++ ) {
			if(bot->buf[i] == '\n') {
				bot->buf[i-1] = '\0';
				break_loop = 1;
				break;
			}
		}
		if ( break_loop )
			break;
	}
	for ( i = 0; i < MESSAGE_LEN; i++) {
		if(bot->buf[i] == '\n') {
			bot->buf[i-1]='\0';
		}
	}
	return 0;
}

int main (int argc, char *argv[])
{
	IRCBot bot1;
	
	init_socket(&bot1, "CBot_s", "irc.eighthbit.net", "6667");
	init_connection(&bot1);
	bot1.chans[0] = "#baddog";
	join(&bot1, bot1.chans[0]);
	
	while(1) {
		fetch_data_from_socket(&bot1);
		parse(&bot1);
	}
	return 0;
}
