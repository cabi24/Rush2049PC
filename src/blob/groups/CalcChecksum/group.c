/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
typedef unsigned char u8;
typedef unsigned int u32;
/* The save files' checksum (MAT3 save.c _CalcChecksum): each byte's low three bits pick how it folds into the sum.
 * Original game: the N64 reads the bytes unsigned; the GameCube build reads them as signed char, so a byte of 0x80
 * or more sign-extends there (`sum |= *ptr` sets the top 24 bits, `sum /= *ptr` divides by a huge unsigned value)
 * and the two versions' checksums of the same data differ. Which one the designers meant is unknown; the N64's is
 * kept, so the PC reads and writes the N64's saves. */
u32 CalcChecksumInternal(u8 *data,u32 size)
{
    u8 *cursor=data;
    u32 result=0,value;
    while(size!=0) {
        value=*cursor;
        switch(value&7) {
        case 0:result-=value;cursor++;break;
        case 1:result|=value;cursor++;break;
        case 2:result&=value;cursor++;break;
        case 3:result^=value;cursor++;break;
        case 4:result*=value;cursor++;break;
        case 5:result/=value;cursor++;break;
        default:result+=value;cursor++;break;
        }
        size--;
    }
    return result;
}

