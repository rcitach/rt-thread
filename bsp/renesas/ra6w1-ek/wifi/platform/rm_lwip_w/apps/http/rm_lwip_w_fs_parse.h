#ifndef __LITTLEFS_PARSE_H__
#define __LITTLEFS_PARSE_H__

#include "common_data.h"

#ifndef FS_FILE_EXTENSION_T_DEFINED
typedef void fs_file_extension;
#endif

struct lfs_file_data {
  const char * data;
  int len;
  int index;
  #if SUPPORT_FSP_RM_FS_W
  lfs_file_t lfs_file;
  #endif
  /* pextension is free for implementations to hold private (extensional)
     arbitrary data, e.g. holding some file state or file system handle */
  fs_file_extension * pextension;
#if HTTPD_PRECALCULATED_CHECKSUM
  const struct fsdata_chksum * chksum;
  u16_t chksum_count;
#endif /* HTTPD_PRECALCULATED_CHECKSUM */
  u8_t flags;
#if LWIP_HTTPD_CUSTOM_FILES
  u8_t is_custom_file;
#endif /* LWIP_HTTPD_CUSTOM_FILES */
#if LWIP_HTTPD_FILE_STATE
  void * state;
#endif /* LWIP_HTTPD_FILE_STATE */
};

#endif // #ifndef __LITTLEFS_PARSE_H__
