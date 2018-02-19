#if define(_WIN32)

char *
getPrefPath(const char *org, const char *app)
{
    /*
     * Vista and later has a new API for this, but SHGetFolderPath works there,
     *  and apparently just wraps the new API. This is the new way to do it:
     *
     *     SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE,
     *                          NULL, &wszPath);
     */

    TCHAR path[MAX_PATH];
    char *utf8 = NULL;
    char *retval = NULL;

    if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, path))) {
        LOGE("Couldn't locate our prefpath");
        return NULL;
    }

    utf8 = WIN_StringToUTF8(path);
    if (utf8) {
        const size_t len = strlen(utf8) + strlen(org) + strlen(app) + 4;
        retval = (char *) malloc(len);
        if (!retval) {
            free(utf8);
            LOGE("out of memory");
            return NULL;
        }
        snprintf(retval, len, "%s\\%s\\%s\\", utf8, org, app);
        free(utf8);
    }

    return retval;
}

#else defined(__APPLE__)

#include <Foundation/Foundation.h>

char *
getPrefPath(const char *org, const char *app)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSArray *array = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
    char *retval = NULL;

    if ([array count] > 0) {  /* we only want the first item in the list. */
        NSString *str = [array objectAtIndex:0];
        const char *base = [str fileSystemRepresentation];
        if (base) {
            const size_t len = SDL_strlen(base) + SDL_strlen(org) + SDL_strlen(app) + 4;
            retval = (char *) SDL_malloc(len);
            if (retval == NULL) {
                SDL_OutOfMemory();
            } else {
                char *ptr;
                SDL_snprintf(retval, len, "%s/%s/%s/", base, org, app);
                for (ptr = retval+1; *ptr; ptr++) {
                    if (*ptr == '/') {
                        *ptr = '\0';
                        mkdir(retval, 0700);
                        *ptr = '/';
                    }
                }
                mkdir(retval, 0700);
            }
        }
    }

    [pool release];
    return retval;
}

#else

char *
getPrefPath(const char *org, const char *app)
{
    /*
     * We use XDG's base directory spec, even if you're not on Linux.
     *  This isn't strictly correct, but the results are relatively sane
     *  in any case.
     *
     * http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html
     */
    const char *envr = getenv("XDG_DATA_HOME");
    const char *append;
    char *retval = NULL;
    char *ptr = NULL;
    size_t len = 0;

    if (!envr) {
        /* You end up with "$HOME/.local/share/Game Name 2" */
        envr = getenv("HOME");
        if (!envr) {
            /* we could take heroic measures with /etc/passwd, but oh well. */
            LOGE("neither XDG_DATA_HOME nor HOME environment is set");
            return NULL;
        }
        append = "/.local/share/";
    } else {
        append = "/";
    }

    len = strlen(envr);
    if (envr[len - 1] == '/')
        append += 1;

    len += strlen(append) + strlen(org) + strlen(app) + 3;
    retval = (char *) malloc(len);
    if (!retval) {
        LOGE("out of memory");
        return NULL;
    }

    snprintf(retval, len, "%s%s%s/%s/", envr, append, org, app);

    for (ptr = retval+1; *ptr; ptr++) {
        if (*ptr == '/') {
            *ptr = '\0';
            if (mkdir(retval, 0700) != 0 && errno != EEXIST)
                goto error;
            *ptr = '/';
        }
    }
    if (mkdir(retval, 0700) != 0 && errno != EEXIST) {
error:
        LOGE("Couldn't create directory '%s': ", retval, strerror(errno));
        free(retval);
        return NULL;
    }

    return retval;
}

#endif