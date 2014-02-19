/*
 * Copyright 2014 MongoDB Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <assert.h>
#include <fcntl.h>

#if defined(__APPLE__)
#include <mach/mach_time.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
# include <sys/types.h>
# include <inttypes.h>
# include <sys/utsname.h>
# include <sys/wait.h>
# include <unistd.h>
# include <sys/time.h>
#else
# include <windows.h>
#endif

#include "TestSuite.h"

#if !defined(_WIN32)
#  include <pthread.h>
#  define TestSuite_mutex_t                 pthread_mutex_t
#  define TestSuite_mutex_init(_n)          pthread_mutex_init((_n), NULL)
#  define TestSuite_mutex_lock              pthread_mutex_lock
#  define TestSuite_mutex_unlock            pthread_mutex_unlock
#  define TestSuite_mutex_destroy           pthread_mutex_destroy
#  define TestSuite_thread_t                pthread_t
#  define TestSuite_thread_create(_t,_f,_d) pthread_create((_t), NULL, (_f), (_d))
#  define TestSuite_thread_join(_n)         pthread_join((_n), NULL)
#  define TestSuite_strdup                  strdup
#  define TestSuite_snprintf                snprintf
#else
#  define TestSuite_mutex_t                    CRITICAL_SECTION
#  define TestSuite_mutex_init                 InitializeCriticalSection
#  define TestSuite_mutex_lock                 EnterCriticalSection
#  define TestSuite_mutex_unlock               LeaveCriticalSection
#  define TestSuite_mutex_destroy              DeleteCriticalSection
#  define TestSuite_thread_t                   HANDLE
static int
TestSuite_thread_create (TestSuite_thread_t *thread,
                         void                *(*cb)(void *),
                         void               *arg)
{
   *thread = CreateThread (NULL, 0, (void *)cb, arg, 0, NULL);
   return 0;
}
#  define TestSuite_thread_join(_n)            WaitForSingleObject ((_n), \
                                                                    INFINITE)
struct timespec { time_t tv_sec; long tv_nsec; };
#  define TestSuite_strdup                     _strdup

int
TestSuite_vsnprintf (char       *str,
                     size_t      size,
                     const char *format,
                     va_list     ap)
{
#ifdef _WIN32
   int r = -1;

   if (size != 0) {
      r = _vsnprintf_s (str, size, _TRUNCATE, format, ap);
   }

   if (r == -1) {
      r = _vscprintf (format, ap);
   }

   return r;
#else
   return vsnprintf (str, size, format, ap);
#endif
}

int
TestSuite_snprintf (char       *str,
                    size_t      size,
                    const char *format,
                    ...)
{
   int r;
   va_list ap;

   va_start (ap, format);
   r = TestSuite_vsnprintf (str, size, format, ap);
   va_end (ap);

   return r;
}

#endif

#define TEST_VERBOSE   (1 << 0)
#define TEST_NOFORK    (1 << 1)
#define TEST_HELPONLY  (1 << 2)
#define TEST_NOTHREADS (1 << 3)


#if !defined(Memory_Barrier)
# ifdef _MSC_VER
#  define Memory_Barrier MemoryBarrier
# else
#  define Memory_Barrier __sync_synchronize
# endif
#endif


#if !defined(AtomicInt_DecrementAndTest)
# if defined(__GNUC__)
#  define AtomicInt_DecrementAndTest(p) (__sync_sub_and_fetch(p, 1) == 0)
# elif defined(_MSC_VER)
#  define AtomicInt_DecrementAndTest(p) (InterlockedDecrement(p) == 0)
# endif
#endif


#define NANOSEC_PER_SEC 1000000000UL


void
_Clock_GetMonotonic (struct timespec *ts) /* OUT */
{
#if defined(__linux__) || \
    defined(__FreeBSD__) || \
    defined(__OpenBSD__) || \
    defined(__NetBSD__) || \
    defined(__DragonFly__)
   clock_gettime (CLOCK_MONOTONIC, ts);
#elif defined(__APPLE__)
   static mach_timebase_info_data_t info = { 0 };
   static double ratio;
   uint64_t atime;

   if (!info.denom) {
      mach_timebase_info(&info);
      ratio = info.numer / info.denom;
   }

   atime = mach_absolute_time() * ratio;
   ts->tv_sec = atime * 1e-9;
   ts->tv_nsec = atime - (ts->tv_sec * 1e9);
#elif defined(_WIN32)
   ULONGLONG ticks = GetTickCount64 ();
   ts->tv_sec = ticks / NANOSEC_PER_SEC;
   ts->tv_nsec = ticks % NANOSEC_PER_SEC;
#else
# warning "Monotonic clock is not yet supported on your platform."
#endif
}


void
_Clock_Subtract (struct timespec *ts, /* OUT */
                 struct timespec *x,  /* IN */
                 struct timespec *y)  /* IN */
{
   struct timespec r;

   ASSERT (x);
   ASSERT (y);

   r.tv_sec = (x->tv_sec - y->tv_sec);

   if ((r.tv_nsec = (x->tv_nsec - y->tv_nsec)) < 0) {
      r.tv_nsec += NANOSEC_PER_SEC;
      r.tv_sec -= 1;
   }

   *ts = r;
}


static void
TestSuite_SeedRand (TestSuite *suite, /* IN */
                    Test *test)       /* IN */
{
   int seed;
   int fd;
   int n_read;

   fd = open ("/dev/urandom", O_RDONLY);
   if (fd != -1) {
      n_read = read (fd, &seed, 4);
      assert (n_read == 4);
   } else {
      seed = time (NULL) * (int)getpid ();
   }

   srand (seed);

   if (fd != -1) {
      close (fd);
   }

   test->seed = seed;
}


void
TestSuite_Init (TestSuite *suite,
                const char *name,
                int argc,
                char **argv)
{
   int i;

   memset (suite, 0, sizeof *suite);

   suite->name = TestSuite_strdup (name);
   suite->flags = 0;
   suite->prgname = TestSuite_strdup (argv [0]);

   for (i = 0; i < argc; i++) {
      if (0 == strcmp ("-v", argv [i])) {
         suite->flags |= TEST_VERBOSE;
      } else if (0 == strcmp ("-f", argv [i])) {
         suite->flags |= TEST_NOFORK;
      } else if (0 == strcmp ("-p", argv [i])) {
         suite->flags |= TEST_NOTHREADS;
      } else if ((0 == strcmp ("-h", argv [i])) ||
                 (0 == strcmp ("--help", argv [i]))) {
         suite->flags |= TEST_HELPONLY;
      } else if ((0 == strcmp ("-l", argv [i]))) {
         if (argc - 1 == i) {
            fprintf (stderr, "-l requires an argument.\n");
            exit (EXIT_FAILURE);
         }
         suite->testname = TestSuite_strdup (argv [++i]);
      }
   }
}


static int
TestSuite_CheckDummy (void)
{
   return 1;
}


void
TestSuite_Add (TestSuite  *suite, /* IN */
               const char *name,  /* IN */
               TestFunc    func)  /* IN */
{
   TestSuite_AddFull (suite, name, func, TestSuite_CheckDummy);
}


void
TestSuite_AddFull (TestSuite  *suite,   /* IN */
                   const char *name,    /* IN */
                   TestFunc    func,    /* IN */
                   int (*check) (void)) /* IN */
{
   Test *test;
   Test *iter;

   test = calloc (1, sizeof *test);
   test->name = TestSuite_strdup (name);
   test->func = func;
   test->check = check;
   test->next = NULL;

   if (!suite->tests) {
      suite->tests = test;
      return;
   }

   for (iter = suite->tests; iter->next; iter = iter->next) { }

   iter->next = test;
}


#if !defined(_WIN32)
static int
TestSuite_RunFuncInChild (TestSuite *suite, /* IN */
                          Test *test)       /* IN */
{
   pid_t child;
   int exit_code = -1;
   int fd;

   if (-1 == (child = fork())) {
      return -1;
   }

   if (!child) {
      fd = open ("/dev/null", O_WRONLY);
      dup2 (fd, STDOUT_FILENO);
      close (fd);
      TestSuite_SeedRand (suite, test);
      test->func ();
      exit (0);
   }

   if (-1 == waitpid (child, &exit_code, 0)) {
      perror ("waitpid()");
   }

   return exit_code;
}
#endif


static void
TestSuite_RunTest (TestSuite *suite,       /* IN */
                   Test *test,             /* IN */
                   TestSuite_mutex_t *mutex, /* IN */
                   int *count)             /* INOUT */
{
   struct timespec ts1;
   struct timespec ts2;
   struct timespec ts3;
   char name[64];
   char buf[256];
   int status;

   TestSuite_snprintf (name, sizeof name, "%s%s", suite->name, test->name);
   name [sizeof name - 1] = '\0';

   if (!test->check || test->check ()) {
      _Clock_GetMonotonic (&ts1);

      /*
       * TODO: If not verbose, close()/dup(/dev/null) for stdout.
       */

#if defined(_WIN32)
      TestSuite_SeedRand (suite, test);
      test->func ();
      status = 0;
#else
      if ((suite->flags & TEST_NOFORK)) {
         TestSuite_SeedRand (suite, test);
         test->func ();
         status = 0;
      } else {
         status = TestSuite_RunFuncInChild (suite, test);
      }
#endif

      _Clock_GetMonotonic (&ts2);
      _Clock_Subtract (&ts3, &ts2, &ts1);

      TestSuite_mutex_lock (mutex);
      TestSuite_snprintf (buf, sizeof buf,
                "    { \"status\": \"%s\", "
                      "\"name\": \"%s\", "
                      "\"seed\": \"%u\", "
                      "\"elapsed\": %u.%09u }%s\n",
               (status == 0) ? "PASS" : "FAIL",
               name,
               test->seed,
               (unsigned)ts3.tv_sec,
               (unsigned)ts3.tv_nsec,
               ((*count) == 1) ? "" : ",");
      buf [sizeof buf - 1] = 0;
      fprintf (stdout, "%s", buf);
      TestSuite_mutex_unlock (mutex);
   } else {
      TestSuite_mutex_lock (mutex);
      TestSuite_snprintf (buf, sizeof buf, "    { \"status\": \"SKIP\", \"name\": \"%s\" },\n", test->name);
      buf [sizeof buf - 1] = '\0';
      fprintf (stdout, "%s", buf);
      TestSuite_mutex_unlock (mutex);
   }
}


static void
TestSuite_PrintHelp (TestSuite *suite, /* IN */
                     FILE *stream)     /* IN */
{
   Test *iter;

   fprintf (stream,
"usage: %s [OPTIONS]\n"
"\n"
"Options:\n"
"    -h, --help   Show this help menu.\n"
"    -f           Do not fork() before running tests.\n"
"    -l NAME      Run test by name.\n"
"    -p           Do not run tests in parallel.\n"
"    -v           Be verbose with logs.\n"
"\n"
"Tests:\n",
            suite->prgname);

   for (iter = suite->tests; iter; iter = iter->next) {
      fprintf (stream, "    %s%s\n", suite->name, iter->name);
   }

   fprintf (stream, "\n");
}


static void
TestSuite_PrintJsonHeader (TestSuite *suite) /* IN */
{
#ifdef _WIN32
#  define INFO_BUFFER_SIZE 32767

   SYSTEM_INFO si;
   DWORD version = 0;
   DWORD major_version = 0;
   DWORD minor_version = 0;
   DWORD build = 0;

   GetSystemInfo(&si);
   version = GetVersion();

   major_version = (DWORD)(LOBYTE(LOWORD(version)));
   minor_version = (DWORD)(HIBYTE(LOWORD(version)));

   if (version < 0x80000000) {
      build = (DWORD)(HIWORD(version));
   }

   fprintf (stdout,
            "{\n"
            "  \"host\": {\n"
            "    \"sysname\": \"Windows\",\n"
            "    \"release\": \"%ld.%ld (%ld)\",\n"
            "    \"machine\": \"%ld\",\n"
            "    \"memory\": {\n"
            "      \"pagesize\": %ld,\n"
            "      \"npages\": %d\n"
            "    }\n"
            "  },\n"
            "  \"options\": {\n"
            "    \"parallel\": \"%s\",\n"
            "    \"fork\": \"%s\"\n"
            "  },\n"
            "  \"tests\": [\n",
            major_version, minor_version, build,
            si.dwProcessorType,
            si.dwPageSize,
            0,
            (suite->flags & TEST_NOTHREADS) ? "false" : "true",
            (suite->flags & TEST_NOFORK) ? "false" : "true");
#else
   struct utsname u;
   uint64_t pagesize;
   uint64_t npages = 0;

   ASSERT (suite);

   if (0 != uname (&u)) {
      perror ("uname()");
      return;
   }

   pagesize = sysconf (_SC_PAGE_SIZE);
#  ifdef __linux__
   npages = sysconf (_SC_PHYS_PAGES);
#  endif

   fprintf (stdout,
            "{\n"
            "  \"host\": {\n"
            "    \"sysname\": \"%s\",\n"
            "    \"release\": \"%s\",\n"
            "    \"machine\": \"%s\",\n"
            "    \"memory\": {\n"
            "      \"pagesize\": %"PRIu64",\n"
            "      \"npages\": %"PRIu64"\n"
            "    }\n"
            "  },\n"
            "  \"options\": {\n"
            "    \"parallel\": \"%s\",\n"
            "    \"fork\": \"%s\"\n"
            "  },\n"
            "  \"tests\": [\n",
            u.sysname,
            u.release,
            u.machine,
            pagesize,
            npages,
            (suite->flags & TEST_NOTHREADS) ? "false" : "true",
            (suite->flags & TEST_NOFORK) ? "false" : "true");
#endif

   fflush (stdout);
}


static void
TestSuite_PrintJsonFooter (void) /* IN */
{
   fprintf (stdout, "  ]\n}\n");
}


typedef struct
{
   TestSuite *suite;
   Test *test;
   TestSuite_mutex_t *mutex;
   int *count;
} ParallelInfo;


static void *
TestSuite_ParallelWorker (void *data) /* IN */
{
   ParallelInfo *info = data;

   ASSERT (info);

   TestSuite_RunTest (info->suite, info->test, info->mutex, info->count);

   if (AtomicInt_DecrementAndTest (info->count)) {
      TestSuite_PrintJsonFooter ();
      exit (0);
   }

   return NULL;
}


static void
TestSuite_RunParallel (TestSuite *suite) /* IN */
{
   ParallelInfo *info;
   TestSuite_thread_t *threads;
   TestSuite_mutex_t mutex;
   Test *test;
   int count = 0;
   int i;

   ASSERT (suite);

   TestSuite_mutex_init (&mutex);

   for (test = suite->tests; test; test = test->next) {
      count++;
   }

   threads = calloc (count, sizeof *threads);

   Memory_Barrier ();

   for (test = suite->tests, i = 0; test; test = test->next, i++) {
      info = calloc (1, sizeof *info);
      info->suite = suite;
      info->test = test;
      info->count = &count;
      info->mutex = &mutex;
      TestSuite_thread_create (&threads [i], TestSuite_ParallelWorker, info);
   }

#ifdef _WIN32
   Sleep (30000);
#else
   sleep (30);
#endif

   fprintf (stderr, "Timed out, aborting!\n");

   abort ();
}


static void
TestSuite_RunSerial (TestSuite *suite) /* IN */
{
   Test *test;
   TestSuite_mutex_t mutex;
   int count = 0;

   TestSuite_mutex_init (&mutex);

   for (test = suite->tests; test; test = test->next) {
      count++;
   }

   for (test = suite->tests; test; test = test->next) {
      TestSuite_RunTest (suite, test, &mutex, &count);
      count--;
   }

   TestSuite_PrintJsonFooter ();

   TestSuite_mutex_destroy (&mutex);
}


static void
TestSuite_RunNamed (TestSuite *suite,     /* IN */
                    const char *testname) /* IN */
{
   TestSuite_mutex_t mutex;
   char name[128];
   Test *test;
   int count = 1;

   ASSERT (suite);
   ASSERT (testname);

   TestSuite_mutex_init (&mutex);

   for (test = suite->tests; test; test = test->next) {
      TestSuite_snprintf (name, sizeof name, "%s%s",
                suite->name, test->name);
      name [sizeof name - 1] = '\0';

      if (0 == strcmp (name, testname)) {
         TestSuite_RunTest (suite, test, &mutex, &count);
      }
   }

   TestSuite_PrintJsonFooter ();

   TestSuite_mutex_destroy (&mutex);
}


int
TestSuite_Run (TestSuite *suite) /* IN */
{
   if ((suite->flags & TEST_HELPONLY)) {
      TestSuite_PrintHelp (suite, stderr);
      return 0;
   }

   TestSuite_PrintJsonHeader (suite);

   if (suite->tests) {
      if (suite->testname) {
         TestSuite_RunNamed (suite, suite->testname);
      } else if ((suite->flags & TEST_NOTHREADS)) {
         TestSuite_RunSerial (suite);
      } else {
         TestSuite_RunParallel (suite);
      }
   } else {
      TestSuite_PrintJsonFooter ();
   }

   return 0;
}


void
TestSuite_Destroy (TestSuite *suite)
{
   Test *test;
   Test *tmp;

   for (test = suite->tests; test; test = tmp) {
      tmp = test->next;
      free (test->name);
      free (test);
   }

   free (suite->name);
   free (suite->prgname);
   free (suite->testname);
}
