cutils_sources := \
	ashmem-dev.c \
	ashmem-host.c \
	native_handle.c \
	process_name.c \
	properties.c \
	safe_iop.c \
	sched_policy.c \
	socket_inaddr_any_server.c \
	socket_local_client.c \
	socket_local_server.c \
	socket_loopback_client.c \
	socket_loopback_server.c \
	sockets.c \
	strlcpy.c \
	threads.c

cutils_objects += $(patsubst %.c,cutils/%.o, $(cutils_sources))

cutils_lib := libcutils.a

all_libs += $(cutils_lib)

$(cutils_lib): $(cutils_objects)