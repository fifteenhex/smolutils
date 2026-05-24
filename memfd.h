#ifndef _SMOLUTILS_MEMFD_H
#define _SMOLUTILS_MEMFD_H

int memfd_create_and_size(const char *name, int *memfd)
{

	int ret;

	ret = memfd_create(name, 0);
	if (ret < 0)
		return ret;

	// TODO put the ftruncate or fallocate here.

	*memfd = ret;

	return 0;
}
#endif  /* _SMOLUTILS_MEMFD_H */
