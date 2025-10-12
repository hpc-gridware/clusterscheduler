/*
 *  Copyright 2022-2024 The OpenSSL Project Authors. All Rights Reserved.
 *
 *  Licensed under the Apache License 2.0 (the "License").  You may not use
 *  this file except in compliance with the License.  You can obtain a copy
 *  in the file LICENSE in the source distribution or at
 *  https://www.openssl.org/source/license.html
 */

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <csignal>

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include "uti/ocs_OpenSSL.h"

#define SOCKET int
#define closesocket(s) close(s)

static const int server_port = 4433;

static volatile bool server_running = true;

static SOCKET create_socket(bool isServer) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("Unable to create socket");
        exit(EXIT_FAILURE);
    }

    if (isServer) {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(server_port);
        addr.sin_addr.s_addr = INADDR_ANY;

        // Reuse the address; good for quick restarts
        int optval = 1;
        if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (void *)&optval, sizeof(optval)) < 0) {
            perror("setsockopt(SO_REUSEADDR) failed");
            exit(EXIT_FAILURE);
        }

        if (bind(s, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
            perror("Unable to bind");
            exit(EXIT_FAILURE);
        }

        if (listen(s, 1) < 0) {
            perror("Unable to listen");
            exit(EXIT_FAILURE);
        }
    }

    return s;
}

static void usage(void)
{
    printf("Usage: test_openssl s\n");
    printf("       --or--\n");
    printf("       test_openssl c ip\n");
    printf("       c=client, s=server, hostname of server\n");
    exit(EXIT_FAILURE);
}

#define BUFFERSIZE 1024
int main(int argc, char **argv)
{
    bool isServer;

    SOCKET server_skt = -1;
    SOCKET client_skt = -1;

    /* used by fgets */
    char buffer[BUFFERSIZE];
    char *txbuf;

    char rxbuf[128];
    size_t rxcap = sizeof(rxbuf);
    int rxlen;

    char *rem_server_name = nullptr;

    struct sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    /* ignore SIGPIPE so that server can continue running when client pipe closes abruptly */
    signal(SIGPIPE, SIG_IGN);

    /* Splash */
    printf("\ntest_openssl : Simple Echo Client/Server : %s : %s\n\n", __DATE__,
    __TIME__);

    /* Need to know if client or server */
    if (argc < 2) {
        usage();
        /* NOTREACHED */
    }
    isServer = (argv[1][0] == 's') ? true : false;
    /* If client get remote server address (could be 127.0.0.1) */
    if (isServer) {
        // we need our hostname, rely on the HOST environment variable
        rem_server_name = getenv("HOST");
        if (rem_server_name == nullptr) {
            fprintf(stderr, "HOST environment variable not set\n");
            exit(EXIT_FAILURE);
        }
    } else {
        if (argc != 3) {
            usage();
            /* NOTREACHED */
        }
        rem_server_name = argv[2];
    }

    DSTRING_STATIC(dstr_error, MAX_STRING_SIZE);
    if (!ocs::uti::OpenSSL::is_openssl_available() && !ocs::uti::OpenSSL::initialize(&dstr_error)) {
        fprintf(stderr, "initializing OpenSSL failed: %s\n", sge_dstring_get_string(&dstr_error));
        exit(EXIT_FAILURE);
    }

    std::string cert_path{};
    std::string key_path{};
    const char *home_dir = getenv("HOME");
    if (home_dir == nullptr) {
        fprintf(stderr, "HOME environment variable not set\n");
        exit(EXIT_FAILURE);
    }
    ocs::uti::OpenSSL::build_cert_path(cert_path, home_dir, rem_server_name, "test_openssl");
    ocs::uti::OpenSSL::build_key_path(key_path, home_dir, rem_server_name, "test_openssl");
    printf("Using certificate: %s\n", cert_path.c_str());
    if (isServer) {
        printf("Using key:         %s\n", key_path.c_str());
    }
    ocs::uti::OpenSSL::OpenSSLContext *openssl_context = ocs::uti::OpenSSL::OpenSSLContext::create(isServer, cert_path, key_path, &dstr_error);
    if (openssl_context == nullptr) {
        fprintf(stderr, "initializing OpenSSL context failed: %s\n", sge_dstring_get_string(&dstr_error));
        exit(EXIT_FAILURE);
    }

    /* If server */
    if (isServer) {
        printf("We are the server on port: %d\n\n", server_port);

        /* Create server socket; will bind with server port and listen */
        server_skt = create_socket(true);

        /*
         * Loop to accept clients.
         * Need to implement timeouts on TCP & SSL connect/read functions
         * before we can catch a CTRL-C and kill the server.
         */
        while (server_running) {
            DSTRING_STATIC(error_dstr, MAX_STRING_SIZE);
            ocs::uti::OpenSSL::OpenSSLConnection *openssl_connection = ocs::uti::OpenSSL::OpenSSLConnection::create(openssl_context, &dstr_error);
            if (openssl_connection == nullptr) {
                fprintf(stderr, SFNMAX "\n", sge_dstring_get_string(&error_dstr));
                exit(EXIT_FAILURE);
            }

            /* Wait for TCP connection from client */
            client_skt = accept(server_skt, (struct sockaddr*) &addr, &addr_len);
            if (client_skt < 0) {
                perror("Unable to accept");
                exit(EXIT_FAILURE);
            }

            printf("Client TCP connection accepted\n");

            // register the client socket with the OpenSSL object
            if (!openssl_connection->set_fd(client_skt, &error_dstr)) {
                fprintf(stderr, SFNMAX "\n", sge_dstring_get_string(&error_dstr));
                exit(EXIT_FAILURE);
            }

            /* Wait for SSL connection from the client */
            if (!openssl_connection->accept(&error_dstr)) {
                fprintf(stderr, SFNMAX "\n", sge_dstring_get_string(&error_dstr));
                server_running = false;
            } else {
                printf("Client SSL connection accepted\n\n");

                /* Echo loop */
                while (true) {
                    /* Get message from client; will fail if client closes connection */
                    rxlen = openssl_connection->read(rxbuf, rxcap, &error_dstr);
                    if (rxlen <= 0) {
                        if (rxlen == 0) {
                            printf("Client closed connection\n");
                        } else {
                            printf("SSL_read returned %d\n", rxlen);
                        }
                        fprintf(stderr, SFNMAX "\n", sge_dstring_get_string(&error_dstr));
                        break;
                    }
                    /* Insure null terminated input */
                    rxbuf[rxlen] = 0;
                    /* Look for kill switch */
                    if (strcmp(rxbuf, "kill\n") == 0) {
                        /* Terminate...with extreme prejudice */
                        printf("Server received 'kill' command\n");
                        server_running = false;
                        break;
                    }
                    /* Show received message */
                    printf("Received: %s", rxbuf);
                    /* Echo it back */
                    if (openssl_connection->write(rxbuf, rxlen, &error_dstr) <= 0) {
                        fprintf(stderr, SFNMAX "\n", sge_dstring_get_string(&error_dstr));
                    }
                }
            }
            if (server_running) {
                // Cleanup for next client
                closesocket(client_skt);
                /*
                 * Set client_skt to -1 to avoid double close when
                 * server_running become false before next accept
                 */
                client_skt = -1;
            }
            // Note: the OpenSSL destructor will be called here and will clean up the SSL and SSL_CTX objects
        }
        printf("Server exiting...\n");
    } else {
        printf("We are the client\n\n");

        DSTRING_STATIC(error_dstr, MAX_STRING_SIZE);
        ocs::uti::OpenSSL::OpenSSLConnection *openssl_connection = ocs::uti::OpenSSL::OpenSSLConnection::create(openssl_context, &dstr_error);
        if (openssl_connection == nullptr) {
            fprintf(stderr, SFNMAX "\n", sge_dstring_get_string(&error_dstr));
            goto exit;
        }

        /*
        * gethostbyname returns a structure including the network address
        * of the specified host.
        */
        struct hostent *hp;
        hp = gethostbyname(rem_server_name);
        if (hp == nullptr) {
            perror("Unable to resolve hostname");
            goto exit;
        }

        /* Create "bare" socket */
        client_skt = create_socket(false);
        /* Set up connect address */
        addr.sin_family = AF_INET;
        //inet_pton(AF_INET, rem_server_name, &addr.sin_addr.s_addr);
        memcpy((char *) &addr.sin_addr, (char *) hp->h_addr, hp->h_length);
        printf("%s -> sin_addr: 0x%X\n", rem_server_name, ntohl(addr.sin_addr.s_addr));
        addr.sin_port = htons(server_port);
        /* Do TCP connect with server */
        if (connect(client_skt, (struct sockaddr*) &addr, sizeof(addr)) != 0) {
            perror("Unable to TCP connect to server");
            goto exit;
        } else {
            printf("TCP connection to server successful\n");
        }

        // register the client socket with the OpenSSL object
        if (!openssl_connection->set_fd(client_skt, &error_dstr)) {
            fprintf(stderr, SFNMAX "\n", sge_dstring_get_string(&error_dstr));
            goto exit;
        }

        /* Set hostname for SNI */
        if (!openssl_connection->set_server_name_for_sni(rem_server_name, &error_dstr)) {
            fprintf(stderr, SFNMAX "\n", sge_dstring_get_string(&error_dstr));
            goto exit;
        }
        /* Now do SSL connect with server */
        if (openssl_connection->connect(&error_dstr)) {
            printf("SSL connection to server successful\n\n");

            /* Loop to send input from keyboard */
            while (true) {
                /* Get a line of input */
                memset(buffer, 0, BUFFERSIZE);
                txbuf = fgets(buffer, BUFFERSIZE, stdin);

                /* Exit loop on error */
                if (txbuf == nullptr) {
                    break;
                }
                /* Exit loop if just a carriage return */
                if (txbuf[0] == '\n') {
                    break;
                }
                /* Send it to the server */
                if (openssl_connection->write(txbuf, strlen(txbuf), &error_dstr) <= 0) {
                    printf("Server closed connection\n");
                    fprintf(stderr, SFNMAX "\n", sge_dstring_get_string(&error_dstr));
                    break;
                }

                /* Wait for the echo */
                rxlen = openssl_connection->read(rxbuf, (int)rxcap, &error_dstr);
                if (rxlen <= 0) {
                    printf("Server closed connection\n");
                    fprintf(stderr, SFNMAX "\n", sge_dstring_get_string(&error_dstr));
                    break;
                } else {
                    /* Show it */
                    rxbuf[rxlen] = 0;
                    printf("Received: %s", rxbuf);
                }
            }
            printf("Client exiting...\n");
        } else {
            printf("SSL connection to server failed\n\n");
            fprintf(stderr, SFNMAX "\n", sge_dstring_get_string(&error_dstr));
        }
    }
exit:
    // cleanup
    // all OpenSSL objects should be (and are) destroyed before we call OpenSSL.cleanup()
    ocs::uti::OpenSSL::cleanup();

    // @todo move into the server / client section above
    if (client_skt != -1)
        closesocket(client_skt);
    if (server_skt != -1)
        closesocket(server_skt);

    printf("test_openssl exiting\n");

    return EXIT_SUCCESS;
}
