static int merge_buffer[5];

void merge(int *array, int start, int middle, int end) {
    int i = start, j = middle + 1, k = start;

    while (i <= middle && j <= end) {
        if (array[i] <= array[j])
            merge_buffer[k++] = array[i++];
        else
            merge_buffer[k++] = array[j++];
    }

    while (i <= middle)
        merge_buffer[k++] = array[i++];
    while (j <= end)
        merge_buffer[k++] = array[j++];

    for (int n = start; n <= end; n++)
        array[n] = merge_buffer[n];
}

void merge_sort(int *array, int start, int end) {
    if (start < end) {
        int middle = (start + end) / 2;
        merge_sort(array, start, middle);
        merge_sort(array, middle + 1, end);
        merge(array, start, middle, end);
    }
}

int main() {
    int arr[5] = {1,4,5,3,2};
    merge_sort(arr, 0, 4);
    return 0;
}
