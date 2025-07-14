/*
 * File:
 *   sssp.c
 * Author(s):
 *   Justin Kopinsky <jkopin@mit.edu>
 * Description:
 *   Single Source Shortest Path using concurrent priority queues
 *
 */

#include "math.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#include "../common/static_initialization.h"
#include "include/numa_pq_sssp_impl.h"
#include "include/numa_pq_4leaders_sssp_impl.h"
#include "include/sssp_smq_impl.h"
#include "include/gc.h"
#include "include/intset.h"
#include "include/linden.h"
// #include "include/sv_lin_impl.h"
// #include "include/sv_teams_impl.h"
#include "include/thread_data.h"


#define MAX_DEPS 10
#define MAX_NODES 9000000
#define MAX_DEG 4

extern ALIGNED(64) uint8_t levelmax[64];
__thread unsigned long *seeds;

ALIGNED(64) uint8_t running[64];

typedef struct graph_node_s {
  int deg;
  slkey_t dist;
  int *adj;
  int *weights;
  int times_processed;
} graph_node_t;

// for CBPQ
#ifdef CBPQ
__thread unsigned long nextr = 1;
#endif

graph_node_t *nodes;

sl_intset_t *set_ds;
pq_t *linden_set;
numa_pq_t *numa_pq_ds;
numa_pq_4_t *numa_pq_4_ds;
smq_t *smq_ds;
int i, c, nb_nodes, nb_edges;
unsigned long effreads, updates, effupds, nb_insertions, nb_nontail_insertions,
    nb_removals, nb_removed, nb_dead_nodes;
thread_data_t *thread_data;
// pthread_t *threads;
pthread_attr_t attr;
barrier_t barrier;
struct timeval start, end_time;
int nb_threads = DEFAULT_NB_THREADS;
int seed = DEFAULT_SEED;
DataStructure ds = UNKNOWN;
const char *input = "";
const char *output = "";
const char *verify_file = "";
const char *reduced_file = "";
int src = -1;
int max_levels = -1;
int max_weight = 0;
int bimodal = 0;
bool output_csv = false;
int size_histogram_granularity = 0;
int counter_tsh = 20;
int counter_max = 35;

slkey_t max_inserted_key;
size_t *key_histogram;
size_t key_histogram_size = 0;

void barrier_init(barrier_t *b, int n) {
  pthread_cond_init(&b->complete, NULL);
  pthread_mutex_init(&b->mutex, NULL);
  b->count = n;
  b->crossing = 0;
}

void barrier_cross(barrier_t *b) {
  pthread_mutex_lock(&b->mutex);
  /* One more thread through */
  b->crossing++;
  /* If not all here, wait */
  if (b->crossing < b->count) {
    // [mfs] cond_wait without a loop --> spurious wakeups?
    pthread_cond_wait(&b->complete, &b->mutex);
  } else {
    pthread_cond_broadcast(&b->complete);
    /* Reset for next time */
    b->crossing = 0;
  }
  pthread_mutex_unlock(&b->mutex);
}

void *sssp(void *thread_data) {
  thread_data_t *d = (thread_data_t *)thread_data;
  int const early_break_threshold = 16 * (int)d->nb_threads;

  /* Create transaction */
  // set_cpu(the_cores[d->id]);
  thread_context::create_context(d->id, cpu_policy::FILL_ONE_HYPERTHREAD_LAST);
  /* Wait on barrier */
  ssalloc_init();

  seeds = seed_rand();

  if (d->ds == NUMA_PQ) { // perform thread-inits
    d->numa_pq_ds->threadInit(d->id);
    if (d->id == 0) { // have one thread perform the initial insert
      slkey_t start_key = 0;
      numa_pq_insert(numa_pq_ds, start_key, src); // initial insert
    }
  } else if (d->ds == NUMA_PQ_4) {
    d->numa_pq_4_ds->threadInit(d->id);
    if (d->id == 0) { // have one thread perform the initial insert // todo: try with only one thread inserting source ??
      slkey_t start_key = 0;
      numa_pq_4_insert(numa_pq_4_ds, start_key, src); // initial insert
    }
  } else if (d->ds == SMQ) {
    d->smq_ds->initThread(d->id);
    if (d->id == 0) {
      std::cout << "inserting source!\n";
      slkey_t start_key = 0;
      smq_insert(smq_ds, start_key, src); // initial insert
    }
  }

  barrier_cross(d->barrier);

  // Begin SSSP

  int fail = 0;
  int cnt = 0;
  //printf("op#: %d\n", cnt);
  while (1) {
    slkey_t node_distance = -1;
    val_t node;

    // if (cnt && (cnt % 10000 == 0)) {
    //   printf("op#: %d\n", cnt);
    // }
    //printf("op#: %d\n", cnt);
    //cnt++;

    ++d->nb_removals;
    if (d->ds == SPRAY) {
      spray_delete_min_key(d->set, &node_distance, &node, d);
    } else if (d->ds == LOTAN) {
      lotan_shavit_delete_min_key(d->set, &node_distance, &node, d);
    } else if (d->ds == LINDEN) {
      node = (val_t)deletemin_key(d->linden_set, &node_distance, d);
    } else if (d->ds == NUMA_PQ) {
      numa_pq_remove(d->numa_pq_ds, &node_distance, &node);
      //printf("(%d) removed %ld, %d\n", d->id, newkey, v);
    } else if (d->ds == NUMA_PQ_4) {
      numa_pq_4_remove(d->numa_pq_4_ds, &node_distance, &node);
    } else if (d->ds == SMQ) {
      node = (val_t)smq_remove(d->smq_ds, &node_distance);
    } else {
      printf("error: no queue selected\n");
      exit(1);
    }

    // todo: check diff here between matthew's impl and og spraylist to better understand what he means below
    if (node_distance == (slkey_t)-1) {
      // list is empty; TODO make sure threads don't quit early
      fail++;
      // TODO: really need a better break condition...

      // [mar] This is still a suboptimal break condition, but it should make
      // the worst behavior much less likely. If this thread has never
      // extracted, and there are "enough" total nodes that it's unlikely that
      // the algorithm will complete before that happens, try harder.

      int break_multiplier = 20;

      if (d->nb_removed == 0 && nb_nodes > early_break_threshold) {
        break_multiplier = 200;
      }

      if (fail > (int)(break_multiplier * d->nb_threads)) {
        //std::cout << d->id << ": breaking...\n";
        break;
      }
      continue;
    }
    fail = 0;

    if (node_distance != nodes[node].dist) {
      //printf("node_distance = %ld, nodes[node].dist = %ld\n", node_distance, nodes[node].dist);
      ++d->nb_dead_nodes;
      continue; // dead node
    }

    nodes[node].times_processed++;
    ++d->nb_removed;

    if (size_histogram_granularity > 0 &&
        d->nb_removed % size_histogram_granularity == 0) {
      printf("After extracting %lu live elements: %lu elements in the priority "
             "queue, current distance from source: %lu\n",
             d->nb_removed, (d->nb_insertions - d->nb_removed + 1),
             node_distance);
    }

    for (int i = 0; i < nodes[node].deg; i++) {
      int v = nodes[node].adj[i];
      int w = nodes[node].weights[i];
      slkey_t dist_v = nodes[v].dist;
      // printf("v=%d dist_v=%d\n", v, dist_v);
      slkey_t newkey = node_distance + w;
      if (dist_v == (slkey_t)-1 || newkey < dist_v) {
        // found better path to v
        int res = ATOMIC_CAS_MB(&nodes[v].dist, dist_v, newkey);
        if (res) {
          if (d->ds == LOTAN || d->ds == SPRAY) {
            // add to queue only if CAS is successful
            sl_add_val(d->set, newkey, v, TRANSACTIONAL);
          } else if (d->ds == LINDEN) {
            insert(d->linden_set, newkey, v);
          } else if (d->ds == NUMA_PQ) {
            numa_pq_insert(d->numa_pq_ds, newkey, v);
          } else if (d->ds == NUMA_PQ_4) {
            numa_pq_4_insert(d->numa_pq_4_ds, newkey, v);
          } else if (d->ds == SMQ) {
            //printf("(%d) inserting %ld, %d\n", d->id, newkey, v);
            smq_insert(d->smq_ds, newkey, v);
          } else {
            std::string msg = "Invalid data structure: ";
            msg += std::to_string(d->ds);
            throw std::invalid_argument(msg);
          }
          d->nb_insertions++;

          // Calculate stats for single-thread-only case
          if (nb_threads == 1) {
            // Calculate tail insertion stats.
            if (newkey < max_inserted_key) {
              nb_nontail_insertions++;
            } else if (newkey > max_inserted_key) {
              max_inserted_key = newkey;
            }

            // Update key histogram.
            if (key_histogram_size > newkey) {
              key_histogram[newkey]++;
            }
          }
        } else {
          i--; // retry
        }
      }
    }
  }

  // End SSSP
  //printf("[%d] returning...\n", d->id);
  pthread_exit(NULL);
  //return NULL;
}

void catcher(int sig) { printf("CAUGHT SIGNAL %d\n", sig); }

void help() {
  using std::cout;
  using std::endl;

  cout << "SSSP (priority queue)" << endl
       << endl
       << "Usage:" << endl
       << "  sssp [options...]" << endl
       << endl
       << "Options:" << endl
       << "  -h  Print this message" << endl
       << "  -D  Data Structure to use" << endl
       << "        spraylist      = Spray List" << endl
       << "        lotan_shavit   = Lotan-Shavit Priority Queue" << endl
       << "        linden         = Linden Priority Queue" << endl
       << "        numa_pq_lin    = Numa-aware priority queue" << endl
       << endl
       << "  -t  Number of threads" << endl
       << "        <int> (default=" << DEFAULT_NB_THREADS << ")" << endl
       << "  -s  RNG seed" << endl
       << "        <int> (0=time-based, default=" << DEFAULT_SEED << ")" << endl
       << "  -i  file to read the graph from (required) " << endl
       << "  -o  file to write the resulting shortest paths to" << endl
       << "  -v  file to verify results against" << endl
       << "  -m  if input graph exceeds max-graph-size, use first "
          "max-graph-size nodes"
       << endl
       << "  -w  if>1, use random edge weights uniformly chosen in [1,w]; fixed"
       << endl
       << "      between trials given fixed seed" << endl
       << "  -b  use random edge weights chosen in [20,30]U[70,80]; fixed"
       << endl
       << "      between trials given fixed seed" << endl
       << "  -c  output comma-separated" << endl
       << "  -g  size histogram granularity (default 0 meaning no histogram)"
       << endl
       << "  -u  index of source node. " << endl
       << "(default: first node with positive out-degree)" << endl
       << "  -r  If set, write graph with unreachable nodes removed to "
          "this path"
       << endl
       << "  -k  key histogram buckets (default 0 meaning no histogram)"
       << endl;
}

void read_configuration(int argc, char **argv) {
  while (1) {
    i = 0;
    c = getopt(argc, argv, "bcD:g:hi:k:m:o:r:s:t:u:v:w:x:z:");

    if (c == -1)
      break;

    switch (c) {
    case 0:
      break;
    case 'h':
    case '?':
      help();
      exit(0);
    case 'D': {
      const std::string_view choice(optarg);
      if (choice == "spraylist")
        ds = SPRAY;
      else if (choice == "lotan_shavit")
        ds = LOTAN;
      else if (choice == "linden")
        ds = LINDEN;
      else if (choice == "numa_pq_lin")
        ds = NUMA_PQ;
      else if (choice == "numa_pq_rel")
        ds = NUMA_PQ_4;
      else if (choice == "smq")
        ds = SMQ;
      else {
        std::string msg = "Invalid data structure: ";
        msg += std::to_string(ds);
        throw std::invalid_argument(msg);
      }
      break;
    }
    case 'w':
      max_weight = atoi(optarg);
      break;
    case 'b':
      bimodal = 1;
      break;
    case 't':
      nb_threads = atoi(optarg);
      break;
    case 's':
      seed = atoi(optarg);
      break;
    case 'i':
      input = optarg;
      break;
    case 'o':
      output = optarg;
      break;
    case 'v':
      verify_file = optarg;
      break;
    case 'r':
      reduced_file = optarg;
      break;
    case 'k':
      key_histogram_size = atoi(optarg);
      break;
    case 'm':
      max_levels = atoi(optarg);
      break;
    case 'c':
      output_csv = true;
      break;
    case 'g':
      size_histogram_granularity = atoi(optarg);
      break;
    case 'u':
      src = atoi(optarg);
      break;
    case 'x':
      counter_tsh = atoi(optarg);
      break;
    case 'z':
      counter_max = atoi(optarg);
      break;
    default:
      exit(1);
    }
  }

  assert(nb_threads > 0);

  if (nb_threads > 1) {
    if (size_histogram_granularity > 0) {
      printf("Size histogram is only available at one thread.\n");
      size_histogram_granularity = 0;
    }

    if (key_histogram_size > 0) {
      printf("Key histogram is only available at one thread.\n");
      key_histogram_size = 0;
    }
  } else if (key_histogram_size > 0) {
    key_histogram = new size_t[key_histogram_size]();
  }

  if (output_csv && !strcmp(verify_file, "") && !strcmp(output, ""))
    printf("Warning: Using CSV output with no output or verify file "
           "provided! If you're using CSV output, you're probably collecting "
           "data, and collected data should be verified for correctness!\n");
}

/// Prints the configuration prior to the operation.
void print_configuration() {
  if (output_csv) {
    std::cout << "(bDimstuw)," << bimodal << "," << to_string(ds) << ","
              << input << "," << max_levels << "," << seed << "," << nb_threads << ","
              << src << "," << max_weight;
  } else {
    printf("Set type             : skip list\n");
    printf("Nb threads           : %d\n", nb_threads);
    printf("Seed                 : %d\n", seed);
    printf("PQ Type              : %s\n", to_string(ds));
    printf("Type sizes           : int=%d/long=%d/ptr=%d/word=%d\n",
           (int)sizeof(int), (int)sizeof(long), (int)sizeof(void *),
           (int)sizeof(uintptr_t));
  }
}

void build_graph() {
  FILE *fp = fopen(input, "r");
  if (fp == nullptr) {
    std::string msg = "Couldn't open file: ";
    msg += input;
    throw std::invalid_argument(msg);
  }

  int res = fscanf(fp, "# Nodes: %d Edges: %d\n", &nb_nodes, &nb_edges);
  if (res != 2) {
    std::string msg = "Invalid result code from fscanf(): ";
    msg += std::to_string(res);
    fclose(fp);
    throw std::runtime_error(msg);
  }
  if (nb_nodes > max_levels && max_levels != -1)
    nb_nodes = max_levels;

#ifndef STATIC
  if ((nodes = (graph_node_t *)malloc(nb_nodes * sizeof(graph_node_t))) ==
      NULL) {
    perror("malloc");
    exit(1);
  }
#endif

  for (i = 0; i < nb_nodes; i++) {
    nodes[i].deg = 0;
    nodes[i].dist = -1;
    nodes[i].times_processed = 0;
  }

  int u, v;
  while (fscanf(fp, "%d %d\n", &u, &v) == 2) {
    //printf("u,v: %d %d\n", u, v);
    if (u >= nb_nodes)
      continue;
    if (v >= nb_nodes)
      continue;
    nodes[u].deg++;
  }

#ifndef STATIC
  for (i = 0; i < nb_nodes; i++) {
    if ((nodes[i].adj = (int *)malloc(nodes[i].deg * sizeof(int))) == NULL) {
      perror("malloc");
      exit(1);
    }
    if ((nodes[i].weights = (int *)malloc(nodes[i].deg * sizeof(int))) ==
        NULL) {
      perror("malloc");
      exit(1);
    }
  }
#endif
  fclose(fp);

  if (src == -1) {
    // If src is set to default value, -1, determine source node automatically.
    // We use the first node (starting from 0) with out-degree > 0.
    do {
      ++src;
    } while (src < nb_nodes && nodes[src].deg == 0);

    if (src == nb_nodes) {
      // Seems pretty pointless to run SSSP on a graph with no edges at all,
      // but whatever, let 'em do it.
      src = 0;
    }
  }

  if (src < 0 || src >= nb_nodes) {
    std::string msg = "Source node out of range. Index: ";
    msg += src;
    msg += ", Number of nodes: ";
    msg += nb_nodes;
    throw std::invalid_argument(msg);
  }

  if (!output_csv)
    printf("Source node index    : %d\n", src);

  nodes[src].dist = 0;

  fp = fopen(input, "r");
  if (fp == nullptr) {
    std::string msg = "Couldn't open file: ";
    msg += input;
    throw std::invalid_argument(msg);
  }
  int tmp;
  res = fscanf(fp, "# Nodes: %d Edges: %d\n", &tmp, &nb_edges);
  if (res != 2) {
    std::string msg = "Invalid result code from fscanf(): ";
    msg += std::to_string(res);
    throw std::runtime_error(msg);
  }

  int *idx;
  if ((idx = (int *)malloc(nb_nodes * sizeof(int))) == NULL) {
    perror("malloc");
    exit(1);
  }
  for (i = 0; i < nb_nodes; i++) {
    idx[i] = 0;
  }

  while (fscanf(fp, "%d %d\n", &u, &v) == 2) {
    if (u >= nb_nodes)
      continue;
    if (v >= nb_nodes)
      continue;
    assert(idx[u] < nodes[u].deg);
    nodes[u].adj[idx[u]] = v;
    if (max_weight > 1) {
      nodes[u].weights[idx[u]] = (rand() % max_weight) + 1;
    } else if (bimodal) {
      if (rand() % 2) {
        nodes[u].weights[idx[u]] = (rand() % 11) + 20;
      } else {
        nodes[u].weights[idx[u]] = (rand() % 11) + 70;
      }
    } else {
      nodes[u].weights[idx[u]] = 1;
    }
    idx[u]++;
  }
  free(idx);
}

/// Performs data-structure-specific initialization.
void init_data_structure() {
  *levelmax = floor_log_2(nb_nodes) + 2;

  // Create configuration object for skipvectors
  // config cfg = config("", "", {}, "");
  // cfg.nthreads = nb_threads;
  // cfg.policy = cpu_policy::FILL_ONE_HYPERTHREAD_LAST;

  // If MAX_TEAMS_T is greater than the thread count, then this will be
  // reduced to the thread count. Essentially, we have one team per thread,
  // but we cap at the number of cores. (Threads in the same core feeding out
  // of the same chunk should be fine.)
  //cfg.nteams = MAX_TEAMS_T;

  slkey_t start_key = 0;

  switch (ds) {
  case NUMA_PQ: {
    int heap_list_size = 50000000;
    int lead_buf_capacity = 50;
    int lead_buf_ideal = 50;
    std::cout << "counter max: " << counter_max << ", counter tsh: " << counter_tsh << "\n";
    numa_pq_ds = new numa_pq_t(heap_list_size, lead_buf_capacity, lead_buf_ideal, nb_threads, counter_tsh, counter_max);
    numa_pq_ds->PQInit();
    // note: initial element inserted later due to thread local variables needed for insertions
    break;
  }

  case NUMA_PQ_4: {
    int heap_list_size = 30000000;
    // int counter_thresh = 20;
    // int counter_max = 35;

    numa_pq_4_ds = new numa_pq_4_t(heap_list_size, nb_threads, counter_tsh, counter_max);
    numa_pq_4_ds->PQInit();
    // note: initial element inserted later due to thread local variables needed for insertions
    break;
  }
  case SMQ: {
    smq_ds = new smq_t(nb_threads);
    break;
  }
    
  case LINDEN: {
    int offset = 32; // not sure what this does
    _init_gc_subsystem();
    linden_set = pq_init(offset);
    start_key = 1; // account for the fact that keys must be positive
    insert(linden_set, start_key, src);
    break;
  }
  case LOTAN:
  case SPRAY: {
    set_ds = sl_set_new();
    sl_add_val(set_ds, start_key, src, TRANSACTIONAL);
    break;
  }

  default:
    // No op
    break;
  }

  // Further initialization
  nodes[src].dist = start_key;
  max_inserted_key = start_key;
  if (key_histogram_size > start_key) {
    key_histogram[start_key]++;
  }
}

void reduce_graph() {
  if (!strcmp(reduced_file, ""))
    return;

  // Go through all nodes. If times_processed is positive, it was reached, so we
  // assign it a new index number. To avoid having this functionality increase
  // the size of the node struct and therefore significantly increase the memory
  // footprint, we write the new index directly back to times_processed, a value
  // we no longer need. If times_processed is 0, it wasn't reached, so it
  // will be removed from the reduced file; the flag -1 indicates this.

  std::cout << "Beginning initial scan for graph reduction." << std::endl;

  int reachable_nodes = 0;
  int reachable_edges = 0;
  for (int i = 0; i < nb_nodes; ++i) {
    if (nodes[i].times_processed > 0) {
      nodes[i].times_processed = reachable_nodes++;
      reachable_edges += nodes[i].deg;
    } else {
      nodes[i].times_processed = -1;
    }
  }

  // Write the file header
  FILE *out = fopen(reduced_file, "w");
  fprintf(out, "# Nodes: %d Edges: %d\n", reachable_nodes, reachable_edges);

  printf("Reachable Nodes: %d Edges: %d\n", reachable_nodes, reachable_edges);
  printf("Source Node's New Index (SAVE THIS): %d\n",
         nodes[src].times_processed);

  // Go through the graph and write all the edges from living nodes to the file.
  for (int i = 0; i < nb_nodes; ++i) {
    int const new_idx = nodes[i].times_processed;

    if (new_idx < 0)
      continue;

    for (int j = 0; j < nodes[i].deg; ++j) {
      int const dest_idx = nodes[i].adj[j];
      int const new_dest_idx = nodes[dest_idx].times_processed;
      fprintf(out, "%d %d\n", new_idx, new_dest_idx);
    }
  }

  fclose(out);
}

/// Dumps the results of the SSSP operation.
void print_results() {
  using std::cout;
  using std::endl;

  bool output_file_provided = strcmp(output, "");
  bool verify_file_provided = strcmp(verify_file, "");
  bool reduce_file_provided = strcmp(reduced_file, "");

  if (output_file_provided) {
    // If an output file was provided, write to that file.
    FILE *out = fopen(output, "w");
    if (out == nullptr) {
      cout << "Couldn't open output file: " << output << endl;
    } else {
      if (!output_csv)
        cout << "Writing output..." << endl;

      for (int i = 0; i < nb_nodes; i++) {
        if (ds == LINDEN && nodes[i].dist != 0xFFFFFFFFFFFFFFFF)
          nodes[i].dist--;
        fprintf(out, "%d %lu\n", i, nodes[i].dist);
      }
      fclose(out);
    }
  } else {
    cout << "No output file provided..." << endl;
  }

  if (verify_file_provided) {
    // If a verify file was provided, check against it.
    FILE *vf = fopen(verify_file, "r");
    if (vf == nullptr) {
      cout << "Couldn't open verify file: " << verify_file << endl;
    } else {
      if (!output_csv)
        cout << "Performing verification..." << endl;

      // Scan the file.
      unsigned long u, v;
      unsigned long ulsize = nb_nodes;
      unsigned long i = 0;
      for (i = 0; i < ulsize; ++i) {
        int res = fscanf(vf, "%lu %lu\n", &u, &v);

        if (res != 2) {
          std::string msg = "On iteration ";
          msg += std::to_string(i);
          msg += ", the line could not be formatted as %d %d.";
          throw std::runtime_error(msg);
        }

        if (u != i) {
          std::string msg = "On iteration ";
          msg += std::to_string(i);
          msg += ", the node index number didn't match; was ";
          msg += std::to_string(u);
          throw std::logic_error(msg);
        }

        if (ds == LINDEN && nodes[i].dist != 0xFFFFFFFFFFFFFFFF)
          nodes[i].dist--;

        if (v != nodes[i].dist) {
          std::string msg = "For node index ";
          msg += std::to_string(i);
          msg += ", the computed distance was wrong. Expected: ";
          msg += std::to_string(v);
          msg += ", Actual: ";
          msg += std::to_string(nodes[i].dist);
          throw std::logic_error(msg);
        }
      }
      if (!output_csv)
        printf("Verification successful!\n");
    }
  } else if (!output_csv) {
    printf("No verification performed (no verify file provided).\n");
  }

  // if (!output_file_provided && !verify_file_provided && !reduce_file_provided &&
  //     !output_csv) {
  //   // No output, verify, or reduce file was provided, and we are not in CSV
  //   // output mode, so fall back on the default behavior: print the results.
  //   for (int i = 0; i < nb_nodes; i++) {
  //     printf("%d %lu\n", i, nodes[i].dist);
  //   }
  // }
}

unsigned long get_set_size() {
  switch (ds) {
  case SPRAY:
  case LOTAN:
    return sl_set_size(set_ds);

  case NUMA_PQ:
    return (unsigned long)numa_pq_ds->getSize();
  case NUMA_PQ_4:
    return (unsigned long)numa_pq_4_ds->getSize();
  case SMQ:
    return (unsigned long)smq_ds->getSize();
  case LINDEN: {
    // [mar] Destructively determine the size of the set by emptying it.
    // For how this method is used, this is fine.
    // (The set should be empty at this point anyway.)
    int result = 0;
    thread_data_t d; // Dummy instance
    while (0 != deletemin(linden_set, &d)) {
      ++result;
    }
    return result;
  }

  default:
    return -1;
  }
}

/// Prints the statistics from the SSSP operation.
void print_stats() {

  if (strcmp(reduced_file, ""))
    // Stats are assumed irrelevant in reduction mode.
    return;

  long nb_processed = 0;
  long unreachable = 0;
  for (int i = 0; i < nb_nodes; i++) {
    nb_processed += nodes[i].times_processed;
    if (nodes[i].times_processed == 0) {
      unreachable++;
    }
  }

  long wasted_work = nb_processed - (nb_nodes - unreachable);
  int duration = (end_time.tv_sec * 1000 + end_time.tv_usec / 1000) -
                 (start.tv_sec * 1000 + start.tv_usec / 1000);

  if (!output_csv)
    printf("duration             : %d\n", duration);

  effreads = 0;
  updates = 0;
  nb_insertions =
      1; // [mar] This accounts for initial insertion of the source node
  nb_removals = 0;
  nb_removed = 0;
  effupds = 0;
  for (i = 0; i < nb_threads; i++) {
    if (!output_csv) {
      printf("Thread %d\n", i);
      printf("  #insertions        : %lu\n", thread_data[i].nb_insertions);
      printf("  #removals          : %lu\n", thread_data[i].nb_removals);
      printf("    #alive           : %lu\n", thread_data[i].nb_removed);
      printf("    #dead            : %lu\n", thread_data[i].nb_dead_nodes);
      printf("    #empty           : %lu\n",
             thread_data[i].nb_removals - thread_data[i].nb_removed - thread_data[i].nb_dead_nodes);
    }
    effreads += (thread_data[i].nb_removals - thread_data[i].nb_removed);
    updates += (thread_data[i].nb_insertions + thread_data[i].nb_removals);
    nb_insertions += thread_data[i].nb_insertions;
    nb_removals += thread_data[i].nb_removals;
    nb_removed += thread_data[i].nb_removed;
    nb_dead_nodes += thread_data[i].nb_dead_nodes;
    effupds += thread_data[i].nb_insertions + thread_data[i].nb_removed;
  }

  if (!output_csv) {
    printf("PQ size              : %lu\n", get_set_size());
    printf("nodes processed      : %ld\n", nb_processed);
    printf("unreachable          : %ld\n", unreachable);
    printf("wasted work          : %ld\n", wasted_work);

    printf("Duration             : %d (ms)\n", duration);
    printf("#ops                 : %lu (%f / s)\n", updates,
           (updates)*1000.0 / duration);

    printf("#eff. upd rate       : %f \n",
           100.0 * effupds / (effupds + effreads));

    printf("#update ops          : ");
    printf("%lu (%f / s)\n", updates, updates * 1000.0 / duration);

    printf("#total removals      : %lu\n", nb_removals);
    printf("   #alive            : %lu\n", nb_removed);
    printf("   #dead             : %lu\n", nb_dead_nodes);
    printf("   #empty            : %lu\n",
           nb_removals - nb_removed - nb_dead_nodes);
    printf("#total insertions    : %lu\n", nb_insertions);
    printf("#net (ins. - rem.)   : %lu\n",
           nb_insertions - nb_removed - nb_dead_nodes);
    if (nb_threads == 1) {
      printf("Nontail insertions   : %lu\n", nb_nontail_insertions);
    }
  }

  if (key_histogram_size > 0) {
    printf("Key Histogram (key, times):\n");
    for (size_t i = 0; i < key_histogram_size; ++i) {
      size_t freq = key_histogram[i];
      if (freq != 0)
        printf("%lu, %lu\n", i, freq);
    }
  }

  if (output_csv) {
    std::cout << ",(dur wasted)," << duration << "," << wasted_work
              << std::endl;
  }
}

int main(int argc, char **argv) {

#ifndef NDEBUG
  printf("NDEBUG is not defined, which is probably not what you want if you're "
         "collecting data!\n");
#endif

  // [mar] i don't get why the coordinator thread would need to be pinned,
  // but... the coordinator thread fills the data structure, so for the sake
  // of sv_teams, this is necessary. We set the coordinator's thread id to 0,
  // same as the first worker thread; the coordinator thread will sleep during
  // the experiment so this is not an issue. It may be better to distribute
  // the initial filling across numa zones, like my microbenchmark does, but
  // that's significant work.

  // set_cpu(the_cores[0]);
  thread_context::create_context(0, cpu_policy::FILL_ONE_HYPERTHREAD_LAST);
  ssalloc_init();
  seeds = seed_rand();

  read_configuration(argc, argv);

  if (seed == 0)
    srand((int)time(0));
  else
    srand(seed);

  print_configuration();

  if ((thread_data = (thread_data_t *)malloc(nb_threads * sizeof(thread_data_t))) ==
      NULL) {
    perror("malloc");
    exit(1);
  }

  pthread_t *threads[nb_threads];
  for (int i = 0; i < nb_threads; ++i) {
    threads[i] = new pthread_t;
  }

  build_graph();

  init_data_structure();

  if (!output_csv) {
    printf("Graph size           : %d\n", nb_nodes);
    printf("Level max            : %d\n", *levelmax);
  }

  // Access set from all threads
  barrier_init(&barrier, nb_threads + 1);
  // pthread_attr_init(&attr);
  // pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  if (!output_csv)
    printf("Creating threads: ");

  for (i = 0; i < nb_threads; i++) {
    if (!output_csv) {
      printf("%d, ", i);
    }  
    thread_data[i].ds = ds;
    thread_data[i].first_remove = -1;
    thread_data[i].nb_insertions = 0;
    thread_data[i].nb_dead_nodes = 0;
    thread_data[i].nb_removals = 0;
    thread_data[i].nb_removed = 0;
    thread_data[i].nb_found = 0;
    thread_data[i].nb_aborts = 0;
    thread_data[i].nb_aborts_locked_read = 0;
    thread_data[i].nb_aborts_locked_write = 0;
    thread_data[i].nb_aborts_validate_read = 0;
    thread_data[i].nb_aborts_validate_write = 0;
    thread_data[i].nb_aborts_validate_commit = 0;
    thread_data[i].nb_aborts_invalid_memory = 0;
    thread_data[i].nb_aborts_double_write = 0;
    thread_data[i].max_retries = 0;
    thread_data[i].nb_threads = nb_threads;
    thread_data[i].seed = rand();
    thread_data[i].seed2 = rand();
    thread_data[i].set = set_ds;
    thread_data[i].barrier = &barrier;
    thread_data[i].failures_because_contention = 0;
    thread_data[i].id = i;

    if (ds == LINDEN) {
      thread_data[i].linden_set = linden_set;
    } else if (ds == NUMA_PQ) {
      thread_data[i].numa_pq_ds = numa_pq_ds;
    } else if (ds == NUMA_PQ_4) {
      thread_data[i].numa_pq_4_ds = numa_pq_4_ds;
    } else if (ds == SMQ) {
      thread_data[i].smq_ds = smq_ds;
    }

    // if (pthread_create(threads[i], &attr, sssp, (void *)(&thread_data[i])) != 0) {
    //   fprintf(stderr, "Error creating thread\n");
    //   exit(1);
    // }
    if (pthread_create(threads[i], NULL, sssp, (void *)(&thread_data[i])) != 0) {
      fprintf(stderr, "Error creating thread\n");
      exit(1);
    }
  }

  // End "creating threads" line
  if (!output_csv)
    printf("\n");

  //pthread_attr_destroy(&attr);

  // Catch some signals
  if (signal(SIGHUP, catcher) == SIG_ERR ||
      signal(SIGTERM, catcher) == SIG_ERR) {
    perror("signal");
    exit(1);
  }

  /* stop = 0; */
  *running = 1;

  // Start threads
  barrier_cross(&barrier);

  if (!output_csv)
    printf("STARTING...\n");
  gettimeofday(&start, NULL);

  // Wait for thread completion
  for (i = 0; i < nb_threads; i++) {
    if (pthread_join(*(threads[i]), NULL) != 0) {
      fprintf(stderr, "Error waiting for thread completion\n");
      exit(1);
    }
    printf("thread %d completed\n", i);
  }
  gettimeofday(&end_time, NULL);
  if (!output_csv)
    printf("STOPPING...\n");

  reduce_graph();
  print_results();
  print_stats();

  // Delete set
  if (ds == SPRAY || ds == LOTAN) {
    sl_set_delete(set_ds);
  } else if (ds == NUMA_PQ) {
    numa_pq_ds->PQDeinit();
  } else if (ds == NUMA_PQ_4) {
    numa_pq_4_ds->PQDeinit();
  }

  for (int i = 0; i < nb_threads; ++i) {
      delete threads[i];
  }
  //free(threads);
  free(thread_data);
  delete[] key_histogram;

  return 0;
}
