#include <mongoc/mongoc-thread-private.h>

#include <TestSuite.h>
#include <test-libmongoc.h>


static void
test_cond_wait(void)
{
   int64_t start, duration_usec;
   bson_mutex_t mutex;
   mongoc_cond_t cond;

   bson_mutex_init(&mutex);
   mongoc_cond_init(&cond);

   bson_mutex_lock(&mutex);
   start = bson_get_monotonic_time();
   mongoc_cond_timedwait(&cond, &mutex, 100);
   duration_usec = bson_get_monotonic_time() - start;
   bson_mutex_unlock(&mutex);

   if (!((50 * 1000 < duration_usec) && (150 * 1000 > duration_usec))) {
      test_error("expected to wait 100ms, waited %" PRId64 "\n", duration_usec / 1000);
   }

   mongoc_cond_destroy(&cond);
   bson_mutex_destroy(&mutex);
}


/* Exercise the same contract with native primitives and a custom backend. */
static bson_once_t contract_once = BSON_ONCE_INIT;
static int contract_once_calls;

static BSON_ONCE_FUN(contract_initialize)
{
   ++contract_once_calls;
   BSON_ONCE_RETURN;
}

typedef struct {
   bson_mutex_t mutex;
   bson_shared_mutex_t shared_mutex;
   mongoc_cond_t ready;
   mongoc_cond_t go;
   int started;
   int completed;
   int counter;
   bool released;
} contract_state;

static BSON_THREAD_FUN(contract_worker, arg)
{
   contract_state *state = arg;
   bson_mutex_lock(&state->mutex);
   ++state->started;
   mongoc_cond_signal(&state->ready);
   while (!state->released) {
      ASSERT_CMPINT(mongoc_cond_wait(&state->go, &state->mutex), ==, 0);
   }
#if defined(MONGOC_ENABLE_DEBUG_ASSERTIONS) && defined(BSON_OS_UNIX)
   if (mcommon_thread_backend_get()) {
      ASSERT(mcommon_mutex_is_locked(&state->mutex));
   }
#endif
   bson_mutex_unlock(&state->mutex);

   for (int i = 0; i < 100; ++i) {
      bson_once(&contract_once, contract_initialize);
      ASSERT_CMPINT(contract_once_calls, ==, 1);
      bson_shared_mutex_lock(&state->shared_mutex);
      ++state->counter;
      bson_shared_mutex_unlock(&state->shared_mutex);
      bson_shared_mutex_lock_shared(&state->shared_mutex);
      ASSERT_CMPINT(state->counter, >, 0);
      bson_shared_mutex_unlock_shared(&state->shared_mutex);
   }
   mcommon_sleep_for(mlib_duration(1, ms));

   bson_mutex_lock(&state->mutex);
   ++state->completed;
   mongoc_cond_signal(&state->ready);
   bson_mutex_unlock(&state->mutex);
   BSON_THREAD_RETURN;
}

static void
contract_wait(contract_state *state, int *count)
{
   int64_t deadline = bson_get_monotonic_time() + 5 * 1000 * 1000;
   while (*count != 3) {
      ASSERT(bson_get_monotonic_time() < deadline);
      int ret = mongoc_cond_timedwait(&state->ready, &state->mutex, 100);
      ASSERT(ret == 0 || mongo_cond_ret_is_timedout(ret));
#if defined(MONGOC_ENABLE_DEBUG_ASSERTIONS) && defined(BSON_OS_UNIX)
      if (mcommon_thread_backend_get()) {
         ASSERT(mcommon_mutex_is_locked(&state->mutex));
      }
#endif
   }
}

static void
test_thread_contract(void)
{
   contract_state state = {0};
   bson_thread_t threads[3];
   bson_mutex_init(&state.mutex);
   bson_shared_mutex_init(&state.shared_mutex);
   mongoc_cond_init(&state.ready);
   mongoc_cond_init(&state.go);
   for (int i = 0; i < 3; ++i) {
      ASSERT_CMPINT(mcommon_thread_create(&threads[i], contract_worker, &state), ==, 0);
   }
   bson_mutex_lock(&state.mutex);
   contract_wait(&state, &state.started);
   state.released = true;
   mongoc_cond_broadcast(&state.go);
   contract_wait(&state, &state.completed);
   int ret = mongoc_cond_timedwait(&state.ready, &state.mutex, 1);
   /* Condition waits may wake spuriously. A fresh condition normally times out;
    * the existing cond_wait test also checks the duration of an actual wait. */
   ASSERT(ret == 0 || mongo_cond_ret_is_timedout(ret));
   bson_mutex_unlock(&state.mutex);
   for (int i = 0; i < 3; ++i) {
      ASSERT_CMPINT(mcommon_thread_join(threads[i]), ==, 0);
   }
   ASSERT_CMPINT(state.counter, ==, 300);
   mongoc_cond_destroy(&state.go);
   mongoc_cond_destroy(&state.ready);
   bson_shared_mutex_destroy(&state.shared_mutex);
   bson_mutex_destroy(&state.mutex);
   ASSERT(!mongoc_set_thread_backend(NULL));
}

void
test_thread_install(TestSuite *suite)
{
   TestSuite_Add(suite, "/Thread/cond_wait", test_cond_wait);
   TestSuite_Add(suite, "/Thread/contract", test_thread_contract);
}
