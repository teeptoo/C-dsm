int address2num(char * addr) {
    return (((long int)(addr-BASE_ADDR))/(PAGE_SIZE));
}
