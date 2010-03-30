/*  simple stupid test: just poke the right values into all the registers that Peek()
 *  different on Linux than on Windows...
 */
int S0_unchecked (struct vfs_dev *dev)
{
	_( Poke (dev, 0x00001fec,      0x21570000, 0x04));
	_( Poke (dev, 0x00001ff0,      0x0001299f, 0x04));
	_( Poke (dev, 0x00001ff4,      0xdbdbdbdb, 0x04));
	_( Poke (dev, 0x00001ffc,      0xd4fb4920, 0x04));
	_( SetParam (dev, 0x0011, 0x0008));
	_( Poke (dev, VFS_DARKEN_CD_1, 0x0000eef0, 0x02));
	_( Poke (dev, VFS_DARKEN_CD_2, 0x0000f0f0, 0x02));
	_( Poke (dev, VFS_GRATING,     0x00000000, 0x01));
	_( Poke (dev, 0x00ff9806,      0x00000010, 0x01));
	_( Poke (dev, VFS_CONTRAST,    0x00000014, 0x01));
	_( Poke (dev, VFS_IMAGE_ABCD,  0x00000031, 0x01));
	_( Poke (dev, VFS_GRATING,     0x00000000, 0x01));
	return 0;
}
