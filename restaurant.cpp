#include "restaurant.h"

// Paul LeClair, 1451166
// Drayton Monkman, 1553293
// November 24, 2017

/*
	Sets *ptr to the i'th restaurant. If this restaurant is already in the cache,
	it just copies it directly from the cache to *ptr. Otherwise, it fetches
	the block containing the i'th restaurant and stores it in the cache before
	setting *ptr to it.
*/
void getRestaurant(restaurant* ptr, int i, Sd2Card* card, RestCache* cache) {
	uint32_t block = REST_START_BLOCK + i/8;
	if (block != cache->cachedBlock) {
		while (!card->readBlock(block, (uint8_t*) cache->block)) {
			Serial.print("readblock failed, try again");
		}
		cache->cachedBlock = block;
	}
	*ptr = cache->block[i%8];
}

// Swaps the two restaurants (which is why they are pass by reference).
void swap(RestDist& r1, RestDist& r2) {
	RestDist tmp = r1;
	r1 = r2;
	r2 = tmp;
}

// Selection sort to sort the restaurants.
void ssort(RestDist restaurants[]) {
	for (int i = NUM_RESTAURANTS-1; i >= 1; --i) {
		int maxId = 0;
		for (int j = 1; j <= i; ++j)
			if (restaurants[j].dist > restaurants[maxId].dist) {
				maxId = j;
			}
		swap(restaurants[i], restaurants[maxId]);
	}
}

// Quick sort to sort the restaurants
void qsort(RestDist restaurants[], int n)
{
	// If n <= 1, then the array is sorted
  if (n <= 1)
	{
    return;
  }

  // Select a pivot
  int pivot = restaurants[n/2].dist;

	// Indices of the entries to be swapped
  int i = 0;
  int j = n - 1;

  // Partition array into sections above and below the pivot
  while (i < j) {
		//avoid a situation where i and j continue swapping indefinitely
		if(restaurants[i].dist == restaurants[j].dist)
		{
			i++;
		}

		// find a value greater than or equal to the pivot
    while (restaurants[i].dist  < pivot) {
        ++i;
    }

		// find a value less than or equal to the pivot
    while (restaurants[j].dist  > pivot) {
        --j;
    }

    // Swap the entries at the lower and upper indices
    swap(restaurants[i], restaurants[j]);
  }

  // Recursively qsort the lower partition
  qsort(restaurants, i);

	// Recursively qsort the upper partition
  qsort(&(restaurants[i + 1]), n - 1 - i);
}

// Computes the manhattan distance between two points (x1, y1) and (x2, y2).
int16_t manhattan(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
	return abs(x1-x2) + abs(y1-y2);
}

/*
	Fetches all restaurants from the card, saves their RestDist information
	in restaurants[], and then sorts them based on their distance to the
	point on the map represented by the MapView.
*/
void getAndSortRestaurants(const MapView& mv, RestDist restaurants[], Sd2Card* card, RestCache* cache) {
	restaurant r;

	// first get all the restaurants and store their corresponding RestDist information.
	for (int i = 0; i < NUM_RESTAURANTS; ++i) {
		getRestaurant(&r, i, card, cache);
		restaurants[i].index = i;
		restaurants[i].dist = manhattan(lat_to_y(r.lat), lon_to_x(r.lon),
																		mv.mapY + mv.cursorY, mv.mapX + mv.cursorX);
	}

	// Now sort them.
	qsort(restaurants, NUM_RESTAURANTS);
}
