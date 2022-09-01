/*!
 * \file hwe_ioctl.h
 * \brief ioctl constants
 *
 */
#ifndef HWE_IOCTL_H_INCLUDED
#define HWE_IOCTL_H_INCLUDED 1

#include "hwe_consts.h"

#define HWEIOCTL_MAGIC                  0xFAECE500

/*! Add a new emulated device.
    arg = interface type (0 = tty, 1 = i2c, etc).
    return: unique device id (positive) or error code (negative).
*/
#define HWEIOCTL_ADD_DEVICE             (HWEIOCTL_MAGIC + 1)

/*! Remove an emulated device.
    arg = unique device id.
    return: error code.
*/
#define HWEIOCTL_UNINSTALL_DEVICE       (HWEIOCTL_MAGIC + 2)

/*! Return the number of request/response pairs.
    arg = unique device id.
    return: number of request/response pairs (zero or positive) or error code (negative).
*/
#define HWEIOCTL_PAIR_COUNT             (HWEIOCTL_MAGIC + 3)

/*! Return the list of request/response pairs.
    arg = pointer to a structure
        {
            int device_id;
            int pair_index;
            char pair[HWE_MAX_PAIR_STR + 1];
        }
    where
        device_id = unique device id;
        pair_index = index of the pair in the list;
        pair = null-terminated ASCII string representing the request-response pair in hexadecimal format;
    return: error code.
*/
#define HWEIOCTL_READ_PAIR              (HWEIOCTL_MAGIC + 4)

/*! Add a new request/response pair.
    arg = pointer to a structure
        {
            int device_id;
            int pair_index;
            char pair[HWE_MAX_PAIR_STR + 1];
        }
    where
        device_id = unique device id;
        pair_index = index of the pair in the list (filled by the function on return);
        pair = null-terminated ASCII string representing the request-response pair in hexadecimal format;
    return: error code.
*/
#define HWEIOCTL_WRITE_PAIR             (HWEIOCTL_MAGIC + 5)

/*! Delete a request/response pair.
    arg = pointer to a structure
        {
            int device_id;
            int pair_index;
        }
    where
        device_id = unique device id;
        pair_index = index of the pair in the list.
    return: error code.
*/
#define HWEIOCTL_DELETE_PAIR            (HWEIOCTL_MAGIC + 6)

/*! Delete all request/response pairs.
    arg = unique device id.
    return: error code (negative).
*/
#define HWEIOCTL_CLEAR_PAIRS            (HWEIOCTL_MAGIC + 7)

struct hweioctl_pair {
	int device_id;
	int pair_index;
	char pair[HWE_MAX_PAIR_STR + 1];
};

#endif /* HWE_IOCTL_H_INCLUDED */
