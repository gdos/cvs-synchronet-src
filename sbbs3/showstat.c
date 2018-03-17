#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "sbbs_status.h"
#include "dirwrap.h"

void usage(void) {
	printf("Usage: showstat <path>\n");
}

int main(int argc, char **argv)
{
	SOCKET sock;
	struct sockaddr_un addr;
	socklen_t addrlen;
	char *buf[4096];
	struct sbbs_status_msg *msg;

	if (argc != 2) {
		usage();
		return 1;
	}
	if (!fexist(argv[1])) {
		usage();
		return 1;
	}

	sock = socket(PF_UNIX, SOCK_SEQPACKET, 0);
	if (sock == INVALID_SOCKET)
		return 1;
	memset(&addr, 0, sizeof(addr));
	SAFECOPY(addr.sun_path, argv[1]);
	addr.sun_family = AF_UNIX;
#ifdef SUN_LEN
	addrlen = SUN_LEN(&addr);
#else
	addrlen = offsetof(struct sockaddr_un, un_addr.sun_path) + strlen(addr.sun_path) + 1;
#endif
	if (connect(sock, (void *)&addr, addrlen) != 0) {
		fprintf(stderr, "Unable to connect!\n");
		return 1;
	}

	send(sock, buf, 1, 0);
	while (sock != -1) {
		if (recv(sock, buf, sizeof(buf), 0) <= 0) {
			close(sock);
			sock = -1;
			continue;
		}
		msg = (void *)buf;
		switch(msg->hdr.service) {
			case SERVICE_EVENT:
				printf("EVENT: ");
				break;
			case SERVICE_FTP:
				printf("FTP: ");
				break;
			case SERVICE_MAIL:
				printf("MAIL: ");
				break;
			case SERVICE_SERVICES:
				printf("SERVICES: ");
				break;
			case SERVICE_TERM:
				printf("TERM: ");
				break;
			case SERVICE_WEB:
				printf("WEB: ");
				break;
		}
		switch(msg->hdr.type) {
			case STATUS_CLIENTS:
				printf("%d clients\n", msg->msg.clients.active);
				break;
			case STATUS_CLIENT_ON:
				printf("got client_on\n");
				break;
			case STATUS_ERRORMSG:
				printf("%d - %s\n", msg->msg.errormsg.level, msg->msg.errormsg.msg);
				break;
			case STATUS_LPUTS:
				printf("%d - %s\n", msg->msg.lputs.level, msg->msg.lputs.msg);
				break;
			case STATUS_RECYCLE:
				printf("recycled\n");
				break;
			case STATUS_SOCKET_OPEN:
				printf("socket %s\n", msg->msg.socket_open.open ? "open" : "close");
				break;
			case STATUS_STARTED:
				printf("started\n");
				break;
			case STATUS_STATS:
				printf("errs: %" PRIu64 ", srv: %" PRIu64 " %s, clients: %d, threads: %d, sockets: %d\n--- %s\n",
					msg->msg.stats.error_count,
					msg->msg.stats.served,
					msg->msg.stats.started ? "running" : "stopped",
					msg->msg.stats.clients,
					msg->msg.stats.threads,
					msg->msg.stats.sockets,
					msg->msg.stats.status);
				break;
			case STATUS_STATUS:
				printf("Status '%s'\n", msg->msg.status.status);
				break;
			case STATUS_TERMINATED:
				printf("Terminated %d\n", msg->msg.terminated.code);
				break;
			case STATUS_THREAD_UP:
				printf("thread %s (%d)\n", msg->msg.thread_up.up ? "up" : "down", msg->msg.thread_up.setuid);
				break;
			default:
				printf("!Unhandled type: %d\b", msg->hdr.type);
				break;
		}
	}
	return 0;
}
