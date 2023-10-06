#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Cache simulator to analyze the performance caches with different configurations
// Accesses, Hits and Hit ratio are printed at the end of the simulation
// Run this to try: ./cache_sim [cache size: 128-4096] [cache mapping: dm|fa] [cache organization: uc|sc]

typedef enum
{
    dm,
    fa
} cache_map_t;
typedef enum
{
    uc,
    sc
} cache_org_t;
typedef enum
{
    instruction,
    data
} access_t;

typedef struct
{
    uint32_t address;
    access_t accesstype;
} mem_access_t;

typedef struct
{
    uint32_t index;
    uint32_t tag;
} parsed_address;

typedef struct
{
    uint64_t accesses;
    uint64_t hits;
} cache_stat_t;

uint32_t cache_size;
uint32_t block_size = 64;
uint32_t num_blocks;
cache_map_t cache_mapping;
cache_org_t cache_org;
uint32_t counter = 0;
uint32_t counter2;
cache_stat_t cache_statistics;

/***
 * Returns a struct with the index and tag bits of the address
 */
parsed_address parse_address(cache_map_t mapping, mem_access_t access, uint32_t num_blocks, uint32_t block_size)
{
    uint32_t num_tag_bits = 32 - (int)(log2(num_blocks)) - (int)(log2(block_size));
    uint32_t num_index_bits = (int)(log2(num_blocks));
    uint32_t num_offset_bits = (int)(log2(block_size));
    parsed_address result;

    // Direct mpping case (Variable sized indexes and tags)
    if (mapping == dm)
    {
        uint32_t index_mask = (0xFFFFFFFF >> (num_tag_bits + num_offset_bits)) << num_offset_bits; // Mask for the index bits
        result.index = (access.address & index_mask) >> num_offset_bits;                           // Use mask and shuft to the right to remove zeros
        result.tag = access.address >> (num_offset_bits + num_index_bits);                         // Shift to the right to be left with tag
    }

    // Fully associative case (Index is always 0)
    else if (mapping == fa)
    {
        result.tag = access.address >> 6;
        result.index = 0;
    }
    return result;
}
/**
 * Initializes the cache as an array of zeros
 */
uint32_t *initialize_cache(uint32_t num_blocks)
{
    uint32_t *cache = (uint32_t *)malloc(num_blocks * sizeof(uint32_t));
    memset(cache, 0, num_blocks * sizeof(uint32_t));

    return cache;
}

/**
 * Accesses a block in the cache. If the block is not in the cache, it is loaded into the cache.
 * 4 cache configurations are supported:
 * If the cache is fully associative, the block is loaded into the first empty slot and FIFO replacement thereafter.
 */

void access_block(uint32_t *cache[], cache_org_t org, cache_map_t mapping, parsed_address parsed, uint32_t num_blocks, mem_access_t access)
{
    uint32_t index = parsed.index;
    uint32_t tag = parsed.tag;

    // Unified direct mapped
    if (org == uc && mapping == dm)
    {
        if (cache[index] == tag)
        {
            cache_statistics.hits++;
        }
        else
        {
            cache[parsed.index] = parsed.tag;
        }
        cache_statistics.accesses++;
        return;
    }
    // Unified fully associative
    else if (org == uc && mapping == fa)
    {
        cache_statistics.accesses++;
        for (int i = 0; i < num_blocks; i++)
        {
            if (cache[i] == tag)
            {
                cache_statistics.hits++;
                return;
            }
            else if (cache[i] == 0)
            {
                cache[i] = tag;
                return;
            }
        }
        cache[counter] = tag;
        if (counter == num_blocks - 1)
        {
            counter = 0;
        }
        else
        {
            counter++;
        }
    }

    // Split direct mapped
    else if (org == sc && mapping == dm)
    {
        uint32_t offset = block_size / 2;

        if (access.accesstype == instruction)
        {
            if (cache[index] == tag)
            {
                cache_statistics.hits++;
            }
            else
            {
                cache[index] = tag;
            }
            cache_statistics.accesses++;
            return;
        }
        else if (access.accesstype == data)
        {
            if (cache[index + offset] == tag)
            {
                cache_statistics.hits++;
            }
            else
            {
                cache[index + offset] = tag;
            }
            cache_statistics.accesses++;
            return;
        }
    }

    // Split fully associative
    else if (org == sc && mapping == fa)
    {
        uint32_t offset = block_size / 2;
        cache_statistics.accesses++;
        if (access.accesstype == 0)
        {
            for (int i = 0; i < offset; i++)
            {
                if (cache[i] == tag)
                {
                    cache_statistics.hits++;
                    return;
                }
                else if (cache[i] == 0)
                {
                    cache[i] = tag;
                    return;
                }
            }
            cache[counter] = tag;
            if (counter >= offset - 1)
            {
                counter = 0;
            }
            else
            {
                counter++;
            }
        }
        else if (access.accesstype == 1)
        {
            for (int i = 0; i < offset; i++)
            {
                if (cache[i + offset] == tag)
                {
                    cache_statistics.hits++;
                    return;
                }
                else if (cache[i + offset] == 0)
                {
                    cache[i + offset] = tag;
                    return;
                }
            }
            cache[counter2] = tag;
            if (counter2 >= num_blocks - 1)
            {
                counter2 = offset;
            }
            else
            {
                counter2++;
            }
        }
    }
}

/* Reads a memory access from the trace file and returns
 * 1) access type (instruction or data access
 * 2) memory address
 */
mem_access_t read_transaction(FILE *ptr_file)
{
    char type;
    mem_access_t access;

    if (fscanf(ptr_file, "%c %x\n", &type, &access.address) == 2)
    {
        if (type != 'I' && type != 'D')
        {
            printf("Unkown access type\n");
            exit(0);
        }
        access.accesstype = (type == 'I') ? instruction : data;
        return access;
    }

    /* If there are no more entries in the file,
     * return an address 0 that will terminate the infinite loop in main
     */
    access.address = 0;
    return access;
}

void main(int argc, char **argv)
{
    uint32_t index_bits;
    counter2 = block_size / 2;
    // Reset statistics:
    memset(&cache_statistics, 0, sizeof(cache_stat_t));

    /* Read command-line parameters and initialize:
     * cache_size, cache_mapping and cache_org variables
     */
    /* IMPORTANT: IF YOU ADD COMMAND LINE PARAMETERS (you really don't need to),
     * MAKE SURE TO ADD THEM IN THE END AND CHOOSE SENSIBLE DEFAULTS SUCH THAT WE
     * CAN RUN THE RESULTING BINARY WITHOUT HAVING TO SUPPLY MORE PARAMETERS THAN
     * SPECIFIED IN THE UNMODIFIED FILE (cache_size, cache_mapping and cache_org)
     */
    if (argc != 4)
    { /* argc should be 2 for correct execution */
        printf(
            "Usage: ./cache_sim [cache size: 128-4096] [cache mapping: dm|fa] "
            "[cache organization: uc|sc]\n");
        exit(0);
    }
    else
    {
        /* argv[0] is program name, parameters start with argv[1] */

        /* Set cache size */
        cache_size = atoi(argv[1]);
        num_blocks = cache_size / block_size;

        /* Set Cache Mapping */
        if (strcmp(argv[2], "dm") == 0)
        {
            cache_mapping = dm;
        }
        else if (strcmp(argv[2], "fa") == 0)
        {
            cache_mapping = fa;
        }
        else
        {
            printf("Unknown cache mapping\n");
            exit(0);
        }

        /* Set Cache Organization */
        if (strcmp(argv[3], "uc") == 0)
        {
            cache_org = uc;
        }
        else if (strcmp(argv[3], "sc") == 0)
        {
            cache_org = sc;
        }
        else
        {
            printf("Unknown cache organization\n");
            exit(0);
        }
    }

    /* Open the file mem_trace.txt to read memory accesses */
    FILE *ptr_file;
    ptr_file = fopen("mem_trace.txt", "r");
    if (!ptr_file)
    {
        printf("Unable to open the trace file\n");
        exit(1);
    }

    /* Loop until whole trace file has been read */
    mem_access_t access;

    uint32_t *cache = initialize_cache(num_blocks);
    while (1)
    {
        access = read_transaction(ptr_file);
        // If no transactions left, break out of loop
        if (access.address == 0)
            break;
        // printf("%d %x\n", access.accesstype, access.address);
        /* Do a cache access */
        // ADD YOUR CODE HERE
        parsed_address parsed = parse_address(cache_mapping, access, num_blocks, block_size);
        access_block(cache, cache_org, cache_mapping, parsed, num_blocks, access);
    }

    /* Print the statistics */
    // DO NOT CHANGE THE FOLLOWING LINES!
    printf("\nCache Statistics\n");
    printf("-----------------\n\n");
    printf("Accesses: %ld\n", cache_statistics.accesses);
    printf("Hits:     %ld\n", cache_statistics.hits);
    printf("Hit Rate: %.4f\n",
           (double)cache_statistics.hits / cache_statistics.accesses);
    // DO NOT CHANGE UNTIL HERE
    // You can extend the memory statistic printing if you like!

    /* Close the trace file */
    fclose(ptr_file);
}
