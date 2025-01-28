#include "core/or/payment_hash.h"

#define _XOPEN_SOURCE 500 /* Enable certain library functions (strdup) on linux.  See feature_test_macros(7) */


#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include "trunnel-impl.h"
#include "lib/malloc/malloc.h"



/* Create a new hashtable. */
hashtable_t* ht_create() {

    hashtable_t *hashtable = NULL;
    int i;

    hashtable = (hashtable_t*)tor_malloc_zero_(sizeof(hashtable_t));
    // Allocate the table itself.
    if(  hashtable  == NULL ) {
        return NULL;
    }
    hashtable->table = (entry_t**)tor_malloc_zero_( sizeof( entry_t *) * 65536);
    // Allocate pointers to the head nodes.
    if( hashtable->table == NULL ) {
        return NULL;
    }
    for( i = 0; i < 65536; i++ ) {
        hashtable->table[i] == NULL;
    }

    return hashtable;
}

// Hash a string for a particular hash table.
int ht_hash( hashtable_t *hashtable, char *key ) {

    unsigned long int hashval;
    int i = 0;

    // Convert our string to an integer
    while( hashval < ULONG_MAX && i < strlen( key ) ) {
        hashval = hashval << 8;
        hashval += key[ i ];
        i++;
    }

    return hashval % 65536;
}

// Create a key-value pair.
entry_t *ht_newpair( char *key, char *value ) {
    entry_t *newpair;
    newpair =  (entry_t *)tor_malloc_zero_(sizeof(entry_t));
    if( newpair == NULL ) {
        return NULL;
    }

    if( ( newpair->key = tor_strndup( key , 100) ) == NULL ) {
        return NULL;
    }

    if( ( newpair->value = tor_strndup( value, 10000 ) ) == NULL ) {
        return NULL;
    }

    newpair->next = NULL;

    return newpair;
}

// Insert a key-value pair into a hash table.
void ht_set( hashtable_t *hashtable, char *key, char *value ) {
    int bin = 0;
    entry_t *newpair = NULL;
    entry_t *next = NULL;
    entry_t *last = NULL;

    bin = ht_hash( hashtable, key );

    next = hashtable->table[ bin ];

    while( next != NULL && next->key != NULL && strcmp( key, next->key ) > 0 ) {
        last = next;
        next = next->next;
    }

    // There's already a pair.  Let's replace that string.
    if( next != NULL && next->key != NULL && strcmp( key, next->key ) == 0 ) {
        tor_free_( next->value );
        next->value = tor_strndup( value , 100000);


        // Nope, could't find it.  Time to grow a pair.
    } else {
        newpair = ht_newpair( key, value );

        // We're at the start of the linked list in this bin.
        if( next == hashtable->table[ bin ] ) {
            newpair->next = next;
            hashtable->table[ bin ] = newpair;

            // We're at the end of the linked list in this bin.
        } else if ( next == NULL ) {
            last->next = newpair;

            // We're in the middle of the list.
        } else  {
            newpair->next = next;
            last->next = newpair;
        }
    }
}


// Retrieve a key-value pair from a hash table.
char *ht_get(hashtable_t *hashtable, char *key ) {
    int bin = 0;
    entry_t *pair;

    bin = ht_hash( hashtable, key );

    // Step through the bin, looking for our value.
    pair = hashtable->table[ bin ];
    while( pair != NULL && pair->key != NULL && strcmp( key, pair->key ) > 0 ) {
        pair = pair->next;
    }

    // Did we actually find anything?
    if( pair == NULL || pair->key == NULL || strcmp( key, pair->key ) != 0 ) {
        return NULL;

    } else {
        return pair->value;
    }

}