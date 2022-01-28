/*
   Copyright (c) 2009 Frank Lahm <franklahm@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/

/*!
 * @file
 * Netatalk utility functions
 */

#include "config.h"

#include <atalk/standards.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <sys/ioctl.h>

#include <atalk/logger.h>
#include <atalk/util.h>

/*!
 * @brief set or unset non-blocking IO on a fd
 *
 * @param     fd         (r) File descriptor
 * @param     cmd        (r) 0: disable non-blocking IO, ie block\n
 *                           <>0: enable non-blocking IO
 *
 * @returns   0 on success, -1 on failure
 */
int setnonblock(int fd, int cmd)
{
	int ofdflags;
	int fdflags;

	if ((fdflags = ofdflags = fcntl(fd, F_GETFL, 0)) == -1)
		return -1;

	if (cmd)
		fdflags |= O_NONBLOCK;
	else
		fdflags &= ~O_NONBLOCK;

	if (fdflags != ofdflags)
		if (fcntl(fd, F_SETFL, fdflags) == -1)
			return -1;

	return 0;
}

/*!
 * non-blocking drop-in replacement for read with timeout using select
 *
 * @param socket          (r)  socket, if in blocking mode, pass "setnonblocking" arg as 1
 * @param data            (rw) buffer for the read data
 * @param lenght          (r)  how many bytes to read
 * @param setnonblocking  (r)  when non-zero this func will enable and disable non blocking
 *                             io mode for the socket
 * @param timeout         (r)  number of seconds to try reading
 *
 * @returns number of bytes actually read or -1 on timeout or error
 */
ssize_t readt(int socket, void *data, const size_t length,
	      int setnonblocking, int timeout)
{
	size_t stored = 0;
	ssize_t len = 0;
	struct timeval now, end, tv;
	fd_set rfds;
	int ret;

	FD_ZERO(&rfds);

	if (setnonblocking) {
		if (setnonblock(socket, 1) != 0)
			return -1;
	}

	/* Calculate end time */
	(void) gettimeofday(&now, NULL);
	end = now;
	end.tv_sec += timeout;

	while (stored < length) {
		len =
		    read(socket, (char *) data + stored, length - stored);
		if (len == -1) {
			switch (errno) {
			case EINTR:
				continue;
			case EAGAIN:
				FD_SET(socket, &rfds);
				tv.tv_usec = 0;
				tv.tv_sec = timeout;

				while ((ret =
					select(socket + 1, &rfds, NULL,
					       NULL, &tv)) < 1) {
					switch (ret) {
					case 0:
						LOG(log_debug,
						    logtype_afpd,
						    "select timeout %d s",
						    timeout);
						errno = EAGAIN;
						goto exit;

					default:	/* -1 */
						switch (errno) {
						case EINTR:
							(void)
							    gettimeofday
							    (&now, NULL);
							if (now.tv_sec >
							    end.tv_sec
							    || (now.
								tv_sec ==
								end.tv_sec
								&& now.
								tv_usec >=
								end.
								tv_usec)) {
								LOG(log_debug, logtype_afpd, "select timeout %d s", timeout);
								goto exit;
							}
							if (now.tv_usec >
							    end.tv_usec) {
								tv.tv_usec
								    =
								    1000000
								    +
								    end.
								    tv_usec
								    -
								    now.
								    tv_usec;
								tv.tv_sec =
								    end.
								    tv_sec
								    -
								    now.
								    tv_sec
								    - 1;
							} else {
								tv.tv_usec
								    =
								    end.
								    tv_usec
								    -
								    now.
								    tv_usec;
								tv.tv_sec =
								    end.
								    tv_sec
								    -
								    now.
								    tv_sec;
							}
							FD_SET(socket,
							       &rfds);
							continue;
						case EBADF:
							/* possibly entered disconnected state, don't spam log here */
							LOG(log_debug,
							    logtype_afpd,
							    "select: %s",
							    strerror
							    (errno));
							stored = -1;
							goto exit;
						default:
							LOG(log_error,
							    logtype_afpd,
							    "select: %s",
							    strerror
							    (errno));
							stored = -1;
							goto exit;
						}
					}
				}	/* while (select) */
				continue;
			}	/* switch (errno) */
			LOG(log_error, logtype_afpd, "read: %s",
			    strerror(errno));
			stored = -1;
			goto exit;
		}		/* (len == -1) */
		else if (len > 0)
			stored += len;
		else
			break;
	}			/* while (stored < length) */

      exit:
	if (setnonblocking) {
		if (setnonblock(socket, 0) != 0)
			return -1;
	}

	if (len == -1 && stored == 0)
		/* last read or select got an error and we haven't got yet anything => return -1 */
		return -1;
	return stored;
}

/*!
 * non-blocking drop-in replacement for read with timeout using select
 *
 * @param socket          (r)  socket, if in blocking mode, pass "setnonblocking" arg as 1
 * @param data            (rw) buffer for the read data
 * @param lenght          (r)  how many bytes to read
 * @param setnonblocking  (r)  when non-zero this func will enable and disable non blocking
 *                             io mode for the socket
 * @param timeout         (r)  number of seconds to try reading
 *
 * @returns number of bytes actually read or -1 on fatal error
 */
ssize_t writet(int socket, void *data, const size_t length,
	       int setnonblocking, int timeout)
{
	size_t stored = 0;
	ssize_t len = 0;
	struct timeval now, end, tv;
	fd_set rfds;
	int ret;

	if (setnonblocking) {
		if (setnonblock(socket, 1) != 0)
			return -1;
	}

	/* Calculate end time */
	(void) gettimeofday(&now, NULL);
	end = now;
	end.tv_sec += timeout;

	while (stored < length) {
		len =
		    write(socket, (char *) data + stored, length - stored);
		if (len == -1) {
			switch (errno) {
			case EINTR:
				continue;
			case EAGAIN:
				FD_ZERO(&rfds);
				FD_SET(socket, &rfds);
				tv.tv_usec = 0;
				tv.tv_sec = timeout;

				while ((ret =
					select(socket + 1, &rfds, NULL,
					       NULL, &tv)) < 1) {
					switch (ret) {
					case 0:
						LOG(log_warning,
						    logtype_afpd,
						    "select timeout %d s",
						    timeout);
						goto exit;

					default:	/* -1 */
						if (errno == EINTR) {
							(void)
							    gettimeofday
							    (&now, NULL);
							if (now.tv_sec >=
							    end.tv_sec
							    && now.
							    tv_usec >=
							    end.tv_usec) {
								LOG(log_warning, logtype_afpd, "select timeout %d s", timeout);
								goto exit;
							}
							if (now.tv_usec >
							    end.tv_usec) {
								tv.tv_usec
								    =
								    1000000
								    +
								    end.
								    tv_usec
								    -
								    now.
								    tv_usec;
								tv.tv_sec =
								    end.
								    tv_sec
								    -
								    now.
								    tv_sec
								    - 1;
							} else {
								tv.tv_usec
								    =
								    end.
								    tv_usec
								    -
								    now.
								    tv_usec;
								tv.tv_sec =
								    end.
								    tv_sec
								    -
								    now.
								    tv_sec;
							}
							FD_ZERO(&rfds);
							FD_SET(socket,
							       &rfds);
							continue;
						}
						LOG(log_error,
						    logtype_afpd,
						    "select: %s",
						    strerror(errno));
						stored = -1;
						goto exit;
					}
				}	/* while (select) */
				continue;
			}	/* switch (errno) */
			LOG(log_error, logtype_afpd, "read: %s",
			    strerror(errno));
			stored = -1;
			goto exit;
		}		/* (len == -1) */
		else if (len > 0)
			stored += len;
		else
			break;
	}			/* while (stored < length) */

      exit:
	if (setnonblocking) {
		if (setnonblock(socket, 0) != 0)
			return -1;
	}

	if (len == -1 && stored == 0)
		/* last read or select got an error and we haven't got yet anything => return -1 */
		return -1;
	return stored;
}

/*!
 * Add a fd to a dynamic pollfd array that is allocated and grown as needed
 *
 * This uses an additional array of struct polldata which stores type information
 * (enum fdtype) and a pointer to anciliary user data.
 *
 * 1. Allocate the arrays with the size of "maxconns" if *fdsetp is NULL.
 * 2. Fill in both array elements and increase count of used elements
 * 
 * @param maxconns    (r)  maximum number of connections, determines array size
 * @param fdsetp      (rw) pointer to callers pointer to the pollfd array
 * @param polldatap   (rw) pointer to callers pointer to the polldata array
 * @param fdset_usedp (rw) pointer to an int with the number of used elements
 * @param fdset_sizep (rw) pointer to an int which stores the array sizes
 * @param fd          (r)  file descriptor to add to the arrays
 * @param fdtype      (r)  type of fd, currently IPC_FD or LISTEN_FD
 * @param data        (rw) pointer to data the caller want to associate with an fd
 */
void fdset_add_fd(int maxconns,
		  struct pollfd **fdsetp,
		  struct polldata **polldatap,
		  int *fdset_usedp,
		  int *fdset_sizep, int fd, enum fdtype fdtype, void *data)
{
	struct pollfd *fdset = *fdsetp;
	struct polldata *polldata = *polldatap;
	int fdset_size = *fdset_sizep;

	LOG(log_debug, logtype_default,
	    "fdset_add_fd: adding fd %i in slot %i", fd, *fdset_usedp);

	if (fdset == NULL) {	/* 1 */
		/* Initialize with space for all possibly active fds */
		fdset = calloc(maxconns, sizeof(struct pollfd));
		if (!fdset)
			exit(EXITERR_SYS);

		polldata = calloc(maxconns, sizeof(struct polldata));
		if (!polldata)
			exit(EXITERR_SYS);

		fdset_size = maxconns;

		*fdset_sizep = fdset_size;
		*fdsetp = fdset;
		*polldatap = polldata;

		LOG(log_debug, logtype_default,
		    "fdset_add_fd: initialized with space for %i conncections",
		    maxconns);
	}

	/* 2 */
	fdset[*fdset_usedp].fd = fd;
	fdset[*fdset_usedp].events = POLLIN;
	polldata[*fdset_usedp].fdtype = fdtype;
	polldata[*fdset_usedp].data = data;
	(*fdset_usedp)++;
}

/*!
 * Remove a fd from our pollfd array
 *
 * 1. Search fd
 * 2a Matched last (or only) in the set ? null it and return
 * 2b If we remove the last array elemnt, just decrease count
 * 3. If found move all following elements down by one
 * 4. Decrease count of used elements in array
 *
 * This currently doesn't shrink the allocated storage of the array.
 *
 * @param fdsetp      (rw) pointer to callers pointer to the pollfd array
 * @param polldatap   (rw) pointer to callers pointer to the polldata array
 * @param fdset_usedp (rw) pointer to an int with the number of used elements
 * @param fdset_sizep (rw) pointer to an int which stores the array sizes
 * @param fd          (r)  file descriptor to remove from the arrays
 */
void fdset_del_fd(struct pollfd **fdsetp,
		  struct polldata **polldatap,
		  int *fdset_usedp, int *fdset_sizep _U_, int fd)
{
	struct pollfd *fdset = *fdsetp;
	struct polldata *polldata = *polldatap;

	if (*fdset_usedp < 1)
		return;

	for (int i = 0; i < *fdset_usedp; i++) {
		if (fdset[i].fd == fd) {	/* 1 */
			if ((i + 1) == *fdset_usedp) {	/* 2a */
				fdset[i].fd = -1;
				memset(&polldata[i], 0,
				       sizeof(struct polldata));
			} else if (i < (*fdset_usedp - 1)) {	/* 2b */
				memmove(&fdset[i], &fdset[i + 1], (*fdset_usedp - i - 1) * sizeof(struct pollfd));	/* 3 */
				memmove(&polldata[i], &polldata[i + 1], (*fdset_usedp - i - 1) * sizeof(struct polldata));	/* 3 */
			}
			(*fdset_usedp)--;
			break;
		}
	}
}

/* Length of the space taken up by a padded control message of length len */
#ifndef CMSG_SPACE
#define CMSG_SPACE(len) (__CMSG_ALIGN(sizeof(struct cmsghdr)) + __CMSG_ALIGN(len))
#endif

/*
 * Receive a fd on a suitable socket
 * @args fd          (r) PF_UNIX socket to receive on
 * @args nonblocking (r) 0: fd is in blocking mode - 1: fd is nonblocking, poll for 1 sec
 * @returns fd on success, -1 on error
 */
int recv_fd(int fd, int nonblocking)
{
	int ret;
	struct msghdr msgh;
	struct iovec iov[1];
	struct cmsghdr *cmsgp = NULL;
	char buf[CMSG_SPACE(sizeof(int))];
	char dbuf[80];
	struct pollfd pollfds[1];

	pollfds[0].fd = fd;
	pollfds[0].events = POLLIN;

	memset(&msgh, 0, sizeof(msgh));
	memset(buf, 0, sizeof(buf));

	msgh.msg_name = NULL;
	msgh.msg_namelen = 0;

	msgh.msg_iov = iov;
	msgh.msg_iovlen = 1;

	iov[0].iov_base = dbuf;
	iov[0].iov_len = sizeof(dbuf);

	msgh.msg_control = buf;
	msgh.msg_controllen = sizeof(buf);

	if (nonblocking) {
		do {
			ret = poll(pollfds, 1, 2000);	/* poll 2 seconds, evtl. multipe times (EINTR) */
		} while (ret == -1 && errno == EINTR);
		if (ret != 1)
			return -1;
		ret = recvmsg(fd, &msgh, 0);
	} else {
		do {
			ret = recvmsg(fd, &msgh, 0);
		} while (ret == -1 && errno == EINTR);
	}

	if (ret == -1) {
		return -1;
	}

	for (cmsgp = CMSG_FIRSTHDR(&msgh); cmsgp != NULL;
	     cmsgp = CMSG_NXTHDR(&msgh, cmsgp)) {
		if (cmsgp->cmsg_level == SOL_SOCKET
		    && cmsgp->cmsg_type == SCM_RIGHTS) {
			return *(int *) CMSG_DATA(cmsgp);
		}
	}

	if (ret == sizeof(int))
		errno = *(int *) dbuf;	/* Rcvd errno */
	else
		errno = ENOENT;	/* Default errno */

	return -1;
}

/*
 * Send a fd across a suitable socket
 */
int send_fd(int socket, int fd)
{
	int ret;
	struct msghdr msgh;
	struct iovec iov[1];
	struct cmsghdr *cmsgp = NULL;
	char *buf;
	size_t size;
	int er = 0;

	size = CMSG_SPACE(sizeof fd);
	buf = malloc(size);
	if (!buf) {
		LOG(log_error, logtype_cnid, "error in sendmsg: %s",
		    strerror(errno));
		return -1;
	}

	memset(&msgh, 0, sizeof(msgh));
	memset(buf, 0, size);

	msgh.msg_name = NULL;
	msgh.msg_namelen = 0;

	msgh.msg_iov = iov;
	msgh.msg_iovlen = 1;

	iov[0].iov_base = &er;
	iov[0].iov_len = sizeof(er);

	msgh.msg_control = buf;
	msgh.msg_controllen = size;

	cmsgp = CMSG_FIRSTHDR(&msgh);
	cmsgp->cmsg_level = SOL_SOCKET;
	cmsgp->cmsg_type = SCM_RIGHTS;
	cmsgp->cmsg_len = CMSG_LEN(sizeof(fd));

	*((int *) CMSG_DATA(cmsgp)) = fd;
	msgh.msg_controllen = cmsgp->cmsg_len;

	do {
		ret = sendmsg(socket, &msgh, 0);
	} while (ret == -1 && errno == EINTR);
	if (ret == -1) {
		LOG(log_error, logtype_cnid, "error in sendmsg: %s",
		    strerror(errno));
		free(buf);
		return -1;
	}
	free(buf);
	return 0;
}
