#define SIZE 50

static int merge_buffer[SIZE]; // global buffer

void merge(int* array, size_t start, size_t middle, size_t end) {
    size_t i, j, k;

    i = start;
    j = middle + 1;
    k = start;

    while (i <= middle && j <= end) {
        if (array[i] <= array[j]) {
            merge_buffer[k++] = array[i];
        } else {
            merge_buffer[k++] = array[j];
        }
    }
    
    while (i <= middle) { 
        merge_buffer[k++] = array[i++]; 
    }

    while (j <= end) { 
        merge_buffer[k++] = array[j++]; 
    }

    for (int i = start; i < end; i++) {
        array[i] = merge_buffer[i];
    }

}

void merge_sort(int* array, size_t start, size_t end) {
    if (start < end) {
        size_t middle = (start + end) / 2;
        merge_sort(array, start, middle);
        merge_sort(array, middle+1, end);
        merge(array, start, middle, end);
    }
} 