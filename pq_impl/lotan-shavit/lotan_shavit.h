#include <string>

#include "intset.h"
#include "../linden/skiplist.h"
#include "../linden/random.h"

__thread unsigned long* seeds;
extern ALIGNED(64) uint8_t levelmax[64];

namespace lotan_shavit_ns {
    class lotan_shavit {
        public:
        sl_intset_t *lotan_ds;

        const int NUM_THREADS;
        const int MAX_KEY;
        lotan_shavit(int num_threads, int max_key) : NUM_THREADS(num_threads), MAX_KEY(max_key) {} // empty constructor

        int is_marked(uintptr_t i) {
            return (int)(i & (uintptr_t)0x01);
        }

        int lotan_shavit_delete_min(val_t *val);
        int lotan_shavit_delete_min_key(slkey_t *key, val_t *val);
        int lotan_fraser_insert(slkey_t key, val_t val);
        void lotan_init();
        void thread_init();


        long debugKeySum();
        bool getValidated();
        std::string getSizeString();

    };
}