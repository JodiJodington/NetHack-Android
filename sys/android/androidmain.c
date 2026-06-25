/* androidmain.c
 * based on unixmain.c
 */

#include "hack.h"
#include "dlb.h"
#include <setjmp.h>

#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pwd.h>
#ifndef O_RDONLY
#include <fcntl.h>
#endif

extern void set_username();

static jmp_buf env;

extern struct passwd *getpwuid( uid_t);
extern struct passwd *getpwnam(const char *);

staticfn boolean whoami(void);
staticfn void process_options(int, char **);

staticfn void wd_message(void);

void get_nhuuid(void) {
#ifdef NHUUID
    if (svn.nhuuid[0]) return;
    char struuid[sizeof svn.nhuuid] = {0};
    uuid_t binuuid;
    uuid_generate_random(binuuid);
    uuid_unparse(binuuid, struuid);
    if (struuid[0]) memcpy(svn.nhuuid, struuid, sizeof svn.nhuuid);
#endif
}

void free_nhuuid(void) {
    memset(&svn.nhuuid, 0, sizeof svn.nhuuid);
}

static char *make_lockname(const char *filename, char *lockname)
{
#  ifdef NO_FILE_LINKS
    Strcpy(lockname, LOCKDIR);
    Strcat(lockname, "/");
    Strcat(lockname, filename);
#  else
    Strcpy(lockname, filename);
#  endif
    Strcat(lockname, "_lock");
    return lockname;
}

void remove_lock_file(const char *filename)
{
    char locknambuf[BUFSZ];
    const char *lockname;

    lockname = make_lockname(filename, locknambuf);
    unlink(lockname);
}

void nethack_exit(int code)
{
    longjmp(env, code);
}

int NetHackMain(int argc, char** argv)
{
    debuglog("Starting NetHack!");

    int val;

    val = setjmp(env);
    if(val)
    {
        debuglog("exiting...");
        return 0;
    }


    NHFILE *nhfp;
    boolean exact_username;
    FILE* fp;

    boolean resuming = FALSE; /* assume new game */

    early_init(argc, argv);

    gh.hname = argv[0];
    svh.hackpid = getpid();
    (void)umask(0777 & ~FCMASK);

    // hack
    // remove dangling locks
    remove_lock_file(RECORD);
    remove_lock_file(HLOCK);
    // make sure RECORD exists
    fp = fopen_datafile(RECORD, "a", SCOREPREFIX);
    fclose(fp);

    choose_windows(DEFAULT_WINDOW_SYS);

    initoptions();

    init_nhwindows(&argc, argv);
    //exact_username = whoami();

    /*
     * It seems you really want to play.
     */
    u.uhp = 1; /* prevent RIP on early quits */

    process_options(argc, argv); /* command line options */

#ifdef DEF_PAGER
    if(!(catmore = nh_getenv("HACKPAGER")) && !(catmore = nh_getenv("PAGER")))
    catmore = DEF_PAGER;
#endif

//#ifdef MAIL
//  getmailstatus();
//#endif
    
    gp.plnamelen = 0;
    plnamesuffix(); /* strip suffix from name; calls askname() */
                    /* again if suffix was whole name */
                    /* accepts any suffix */
#ifdef WIZARD
    if(!wizard)
#endif
    set_username();

    Sprintf(gl.lock, "%d%s", (int)getuid(), svp.plname);
    getlock();

    /* Set up level 0 file to keep the game state.
     */
    nhfp = create_levelfile(0, (char *)0);
    if(!nhfp)
    {
        raw_print("Cannot create lock file");
    }
    else
    {
        svh.hackpid = 1;
        Sfo_int(nhfp, &svh.hackpid, "svh.hackpid");
        close_nhfile(nhfp);
    }

    dlb_init(); /* must be before newgame() */

    /*
     * Initialization of the boundaries of the mazes
     * Both boundaries have to be even.
     */
    gx.x_maze_max = COLNO - 1;
    if(gx.x_maze_max % 2)
        gx.x_maze_max--;
    gy.y_maze_max = ROWNO - 1;
    if(gy.y_maze_max % 2)
        gy.y_maze_max--;

    /*
     *  Initialize the vision system.  This must be before mklev() on a
     *  new game or before a level restore on a saved game.
     */
    vision_init();

    init_sound_disp_gamewindows();

    if((nhfp = restore_saved_game()) != 0)
    {
#ifdef WIZARD
        /* Since wizard is actually flags.debug, restoring might
         * overwrite it.
         */
        boolean remember_wiz_mode = wizard;
#endif
        const char *fq_save = fqname(gs.SAVEF, SAVEPREFIX, 1);
        int fq_save_fd = open(fq_save, O_RDONLY);
        if (fq_save_fd == -1) {
            debuglog("failed to open save file: %s", strerror(errno));
            goto backup_error;
        }
        struct stat sb;
        if (fstat(fq_save_fd, &sb) < 0) {
            debuglog("failed to stat save file: %s", strerror(errno));
            goto backup_error;
        }
        size_t fq_save_length = sb.st_size;
        char *fq_save_contents = mmap(NULL, fq_save_length, PROT_READ, MAP_PRIVATE,  fq_save_fd, 0);
        if (fq_save_contents == MAP_FAILED) {
            debuglog("failed to mmap save file: %s", strerror(errno));
            goto backup_error;
        } else goto backup_clean;
backup_error:
        debuglog("WARNING: failed to make a backup save file. See messages above for more info.");
backup_clean:
        if (fq_save_fd != -1) close(fq_save_fd);
#ifdef NEWS
        if(iflags.news)
        {
            display_file(NEWS, FALSE);
            iflags.news = FALSE; /* in case dorecover() fails */
        }
#endif
        if (ge.early_raw_messages)
            raw_print("Restoring save file...");
        else
            pline("Restoring save file...");
        mark_synch(); /* flush output */
        if(!dorecover(nhfp)) {
            pline("You may have a backup save file in your save directory. Make a backup and then remove the .bak suffix and restart the app to try again.");
            pline("if you have any questions, open up a github issue, I would be happy to help. Unfortunately this process is probably not automatable in a good way.");
            goto not_recovered;
        }
        if (fq_save_contents) {
            char *fq_save_backup = (char *)alloc(strlen(fq_save) + 4 + 1);
            if (!fq_save_backup) {
                debuglog("OOM");
                goto write_backup_error;
            }
            Sprintf(fq_save_backup, "%s.bak", fq_save);
            int fq_save_backup_fd = creat(fq_save_backup, FCMASK);
            if (fq_save_backup_fd == -1) {
                debuglog("failed to open save backup file: %s", strerror(errno));
                goto write_backup_error;
            }
            size_t written_total = 0;
            while (written_total < fq_save_length) {
                ssize_t written_just_now = write(fq_save_backup_fd, fq_save_contents + written_total, fq_save_length - written_total);
                if (written_just_now < 0) {
                    if (errno == EINTR) continue;
                    debuglog("failed to write backup file: %s", strerror(errno));
                    goto write_backup_error;
                } else if (written_just_now == 0) {
                    debuglog("unexpected EOF in save file when writing backup save file");
                    goto write_backup_error;
                } else {
                    written_total += written_just_now;
                }
            }
            debuglog("finished writing backup file");
            goto write_backup_cleanup;
write_backup_error:
            debuglog("WARNING: failed to write a backup save file. See logs above for more information");
write_backup_cleanup:
            munmap(fq_save_contents, fq_save_length);
            if (fq_save_backup) free(fq_save_backup);
            if (fq_save_backup_fd) close(fq_save_backup_fd);
        }
        resuming = TRUE;
#ifdef WIZARD
        if(!wizard && remember_wiz_mode)
            wizard = TRUE;
#endif
        check_special_room(FALSE);
        wd_message();

        if(discover || wizard)
        {
            if(y_n("Do you want to keep the save file?") == 'n')
            {
                (void)delete_savefile();
            }
            else
            {
                nh_compress(fq_save);
            }
        }
    }
    else
    {
        not_recovered: player_selection();
        resuming = FALSE;
        newgame();
        wd_message();
    }

    moveloop(resuming);
    exit(EXIT_SUCCESS);

    return (0);
}

boolean authorize_wizard_mode()
{
    return TRUE;
}

boolean authorize_explore_mode(void) {
    return TRUE;
}


staticfn void process_options(int argc, char *argv[])
{
    int i;

    /*
     * Process options.
     */
    while(argc > 1 && argv[1][0] == '-')
    {
        argv++;
        argc--;
        switch(argv[0][1])
        {
        case 'D':
#ifdef WIZARD
            wizard = TRUE;
        break;
#endif
        /* otherwise fall thru to discover */
        case 'X':
            discover = TRUE;
        break;
#ifdef NEWS
            case 'n':
            iflags.news = FALSE;
            break;
#endif
        case 'u':
            if(!*svp.plname)
            {
                if(argv[0][2])
                    (void)strncpy(svp.plname, argv[0] + 2, sizeof(svp.plname) - 1);
                else if(argc > 1)
                {
                    argc--;
                    argv++;
                    (void)strncpy(svp.plname, argv[0], sizeof(svp.plname) - 1);
                }
                else
                    raw_print("Player name expected after -u");
            }
        break;
        case 'p': /* profession (role) */
            if(argv[0][2])
            {
                if((i = str2role(&argv[0][2])) >= 0)
                    flags.initrole = i;
            }
            else if(argc > 1)
            {
                argc--;
                argv++;
                if((i = str2role(argv[0])) >= 0)
                    flags.initrole = i;
            }
        break;
        case 'r': /* race */
            if(argv[0][2])
            {
                if((i = str2race(&argv[0][2])) >= 0)
                    flags.initrace = i;
            }
            else if(argc > 1)
            {
                argc--;
                argv++;
                if((i = str2race(argv[0])) >= 0)
                    flags.initrace = i;
            }
        break;
        case '@':
            flags.randomall = 1;
        break;
        default:
            if((i = str2role(&argv[0][1])) >= 0)
            {
                flags.initrole = i;
                break;
            }
            /* else raw_printf("Unknown option: %s", *argv); */
        }
    }
}

#ifdef CHDIR
void chdirx(const char *dir, boolean wr) {
    if (!dir) goto exit;
    if (chdir(dir) < 0) {
        perror(dir);
        goto exit;
    }
    return;
exit:
    error("Cannot chdir to %s.", dir);
}
#endif

staticfn boolean whoami(void)
{
    /*
     * Who am i? Algorithm: 1. Use name as specified in NETHACKOPTIONS
     *          2. Use getlogin()       (if 1. fails)
     * The resulting name is overridden by command line options.
     * If everything fails, or if the resulting name is some generic
     * account like "games", "play", "player", "hack" then eventually
     * we'll ask him.
     * Note that we trust the user here; it is possible to play under
     * somebody else's name.
     */
    register char *s;

    if(*svp.plname)
        return FALSE;
    if((s = getlogin()))
        (void)strncpy(svp.plname, s, sizeof(svp.plname) - 1);
    return TRUE;
}

#ifdef PORT_HELP
void
port_help()
{
    /*
     * Display unix-specific help.   Just show contents of the helpfile
     * named by PORT_HELP.
     */
    display_file(PORT_HELP, TRUE);
}
#endif

staticfn void wd_message(void)
{
    if(discover)
        You("are in non-scoring discovery mode.");
}

/*
 * Add a slash to any name not ending in /. There must
 * be room for the /
 */
void append_slash(char *name)
{
    char *ptr;

    if(!*name)
        return;
    ptr = name + (strlen(name) - 1);
    if(*ptr != '/')
    {
        *++ptr = '/';
        *++ptr = '\0';
    }
    return;
}

unsigned long
sys_random_seed()
{
    unsigned long seed = 0L;
    unsigned long pid = (unsigned long) getpid();
    boolean no_seed = TRUE;
#ifdef DEV_RANDOM
    FILE *fptr;

    fptr = fopen(DEV_RANDOM, "r");
    if (fptr) {
        fread(&seed, sizeof (long), 1, fptr);
        has_strong_rngseed = TRUE;  /* decl.c */
        no_seed = FALSE;
        (void) fclose(fptr);
    } else {
        /* leaves clue, doesn't exit */
        paniclog("sys_random_seed", "falling back to weak seed");
    }
#endif
    if (no_seed) {
        seed = (unsigned long) getnow(); /* time((TIME_type) 0) */
        /* Quick dirty band-aid to prevent PRNG prediction */
        if (pid) {
            if (!(pid & 3L))
                pid -= 1L;
            seed *= pid;
        }
    }
    return seed;
}
