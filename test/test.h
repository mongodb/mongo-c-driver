#define ASSERT(x) \
    do{ \
        if(!(x)){ \
            printf("failed assert (%d): %s\n", __LINE__,  #x); \
            exit(1); \
        }\
    }while(0)

#define INIT_SOCKETS_FOR_WINDOWS \
    { \
        WSADATA out; \
        WSAStartup(MAKEWORD(2,2), &out); \
    }

