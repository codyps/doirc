#ifndef STAG_H_
#define STAG_H_

#include <ccan/short_types/short_types.h>


/* low overhead, tagged encoding with per-item optional metadata */



/* Usage flow:
 * 1. start a container
 * 2. optionally add metadata to that container (id, name, version, etc.)
 * 3. add contents to the element (which may be more elements)
 * 4. end the container
 *
 * Allow quick buildup of an emitted structure, without requiring that data remain in memory.
 * Elements can contain a sequence other elements, a sequence of bytes (string), or an integer.
 *
 *
 * The more I write this, the more I think I should just use msgpack.
 */


#endif
