#include "fs.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>

bool fs_open(const char* path, OpenMode mode, fs_handle* file) {
  int flags;
  switch (mode) {
    case OPEN_READ: flags = O_RDONLY; break;
    case OPEN_WRITE: flags = O_WRONLY | O_CREAT | O_TRUNC; break;
    case OPEN_APPEND: flags = O_WRONLY | O_CREAT; break;
    default: return false;
  }
  int fd = open(path, flags, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    return false;
  }
  file->handle = fd;
  return true;
}

bool fs_close(fs_handle file) {
  return close(file.handle) == 0;
}

bool fs_read(fs_handle file, void* buffer, size_t* bytes) {
  ssize_t result = read(file.handle, buffer, *bytes);
  if (result < 0) {
    *bytes = 0;
    return false;
  } else {
    *bytes = (size_t) result;
    return true;
  }
}

bool fs_write(fs_handle file, const void* buffer, size_t* bytes) {
  ssize_t result = write(file.handle, buffer, *bytes);
  if (result < 0) {
    *bytes = 0;
    return false;
  } else {
    *bytes = (size_t) result;
    return true;
  }
}

void* fs_map(const char* path, size_t* size) {
  FileInfo info;
  fs_handle file;
  if (!fs_stat(path, &info) || !fs_open(path, OPEN_READ, &file)) {
    return NULL;
  }
  *size = info.size;
  void* data = mmap(NULL, *size, PROT_READ, MAP_PRIVATE, file.handle, 0);
  fs_close(file);
  return data;
}

bool fs_unmap(void* data, size_t size) {
  return munmap(data, size) == 0;
}

bool fs_stat(const char* path, FileInfo* info) {
  struct stat stats;
  if (stat(path, &stats)) {
    return false;
  }

  if (info) {
    info->size = (uint64_t) stats.st_size;
    info->lastModified = (uint64_t) stats.st_mtime;
    info->type = (stats.st_mode & S_IFDIR) ? FILE_DIRECTORY : FILE_REGULAR;
  }

  return true;
}

bool fs_remove(const char* path) {
  return unlink(path) == 0 || rmdir(path) == 0;
}

bool fs_mkdir(const char* path) {
  return mkdir(path, S_IRWXU) == 0;
}

bool fs_list(const char* path, fs_list_cb* callback, void* context) {
  DIR* dir = opendir(path);
  if (!dir) {
    return false;
  }

  struct dirent* entry;
  while ((entry = readdir(dir)) != NULL) {
    callback(context, entry->d_name);
  }

  closedir(dir);
  return true;
}

static size_t copy(char* buffer, size_t size, const char* string, size_t length) {
  if (length >= size) { return 0; }
  memcpy(buffer, string, length);
  buffer[length + 1] = '\0';
  return length;
}

size_t fs_getHomeDir(char* buffer, size_t size) {
  const char* home = getenv("HOME");

  if (!home) {
    struct passwd* entry = getpwuid(getuid());
    if (!entry) {
      return 0;
    }
    home = entry->pw_dir;
  }

  return copy(buffer, size, home, strlen(home));
}

size_t fs_getDataDir(char* buffer, size_t size) {
#if __APPLE__
  size_t cursor = fs_getHomeDir(buffer, size);

  if (cursor > 0) {
    const char* suffix = "/Library/Application Support";
    return cursor + copy(buffer + cursor, size - cursor, suffix, strlen(suffix));
  }

  return 0;
#elif EMSCRIPTEN
  const char* path = "/home/web_user";
  return copy(buffer, size, path, strlen(path));
#else
  const char* xdg = getenv("XDG_DATA_HOME");

  if (xdg) {
    return copy(buffer, size, xdg, strlen(xdg));
  } else {
    size_t cursor = fs_getHomeDir(buffer, size);
    if (cursor > 0) {
      const char* suffix = "/.local/share";
      return cursor + copy(buffer + cursor, size - cursor, suffix, strlen(suffix));
    }
  }

  return 0;
#endif
}

size_t fs_getWorkDir(char* buffer, size_t size) {
  return getcwd(buffer, size) ? strlen(buffer) : 0;
}

size_t fs_getExecutablePath(char* buffer, size_t size) {
#if __APPLE_
  return _NSGetExecutablePath(buffer, &size) ? 0 : size;
#elif EMSCRIPTEN
  return 0;
#else
  ssize_t length = readlink("/proc/self/exe", buffer, size);
  return (length < 0) ? 0 : (size_t) length;
#endif
}

#ifdef __APPLE__
#include <objc/objc-runtime.h>
#endif

size_t fs_getBundlePath(char* buffer, size_t size) {
#ifdef __APPLE__
  id extension = ((id(*)(Class, SEL, char*)) objc_msgSend)(objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), "lovr");
  id bundle = ((id(*)(Class, SEL)) objc_msgSend)(objc_getClass("NSBundle"), sel_registerName("mainBundle"));
  id path = ((id(*)(id, SEL, char*, id)) objc_msgSend)(bundle, sel_registerName("pathForResource:ofType:"), nil, extension);
  if (path == nil) {
    return 0;
  }

  const char* cpath = ((const char*(*)(id, SEL)) objc_msgSend)(path, sel_registerName("UTF8String"));
  if (!cpath) {
    return 0;
  }

  size_t length = strlen(cpath);
  if (length >= size) {
    return 0;
  }

  memcpy(buffer, cpath, length);
  buffer[length] = '\0';
  return length;
#else
  return fs_getExecutablePath(buffer, size);
#endif
}

size_t fs_getBundleId(char* buffer, size_t size) {
#ifdef __ANDROID__
  // TODO
  pid_t pid = getpid();
  char path[32];
  snprintf(path, LOVR_PATH_MAX, "/proc/%i/cmdline", (int) pid);
  FILE* file = fopen(path, "r");
  if (file) {
    size_t read = fread(dest, 1, size, file);
    fclose(file);
    return true;
  }
  return 0;
#else
  return 0;
#endif
}