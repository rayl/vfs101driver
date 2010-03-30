int S2_unchecked (struct vfs_dev *dev)
{
	_(  LoadImage (dev));
	_(  GetParam (dev, 0x0014));
	_(  AbortPrint (dev));
	_(  LoadImage (dev));
	_(  GetParam (dev, 0x0011));
	_(  SetParam (dev, 0x0062, info_line_rate));
	_(  GetPrint (dev, 0x0014, type_0));
	_(  LoadImage (dev));
	_(  GetConfig (dev));
	_(  GetParam (dev, 0x002e));
	_(  GetVersion (dev));
	_(  AbortPrint (dev));
	_(  LoadImage (dev));
	_(  SetParam (dev, 0x0055, 0x0008));
	_(  SetParam (dev, 0x0055, 0x0008));
	_(  SetParam (dev, 0x0055, 0x0008));
	_(  GetParam (dev, 0x0014));
	_(  GetParam (dev, 0x0011));
	_(  SetParam (dev, 0x0062, info_line_rate));
	_(  GetPrint (dev, 0x0014, type_0));
	_(  LoadImage (dev));
	_(  GetParam (dev, 0x0014));
	_(  GetParam (dev, 0x0014));
	_(  AbortPrint (dev));
	_(  LoadImage (dev));
	_(  GetParam (dev, 0x0011));
	_(  SetParam (dev, 0x0062, info_line_rate));
	_(  GetPrint (dev, 0x1388, type_1));
	return 0;
}
int S2_checked (struct vfs_dev *dev)
{
	 _(          LoadImage (dev));
	__(  402,    GetParam (dev, 0x0014));
	__(  404,    AbortPrint (dev));
	 _(          LoadImage (dev));
	__(  407,    GetParam (dev, 0x0011));
	__(  409,    SetParam (dev, 0x0062, info_line_rate));
	__(  411,    GetPrint (dev, 0x0014, type_0));
	 _(          LoadImage (dev));
	__(  414,    GetConfig (dev));
	__(  416,    GetParam (dev, 0x002e));
	__(  418,    GetVersion (dev));
	__(  420,    AbortPrint (dev));
	 _(          LoadImage (dev));
	__(  423,    SetParam (dev, 0x0055, 0x0008));
	__(  425,    SetParam (dev, 0x0055, 0x0008));
	__(  427,    SetParam (dev, 0x0055, 0x0008));
	__(  429,    GetParam (dev, 0x0014));
	__(  431,    GetParam (dev, 0x0011));
	__(  433,    SetParam (dev, 0x0062, info_line_rate));
	__(  435,    GetPrint (dev, 0x0014, type_0));
	 _(          LoadImage (dev));
	__(  438,    GetParam (dev, 0x0014));
	__(  440,    GetParam (dev, 0x0014));
	__(  442,    AbortPrint (dev));
	 _(          LoadImage (dev));
	__(  445,    GetParam (dev, 0x0011));
	__(  447,    SetParam (dev, 0x0062, info_line_rate));
	__(  449,    GetPrint (dev, 0x1388, type_1));
	return 0;
}
struct result_table S2_results =
{
	449,
	{
	[   402 ] = {   6, "\x04\x00\x00\x00\x00\x00" },
	[   404 ] = {   4, "\x0e\x00\x00\x00" },
	[   407 ] = {   6, "\x04\x00\x00\x00\x08\x00" },
	[   409 ] = {   6, "\x05\x00\x00\x00\x32\x00" },
	[   411 ] = {   4, "\x03\x00\x00\x00" },
	[   414 ] = {  34, "\x06\x00\x00\x00\x00\x00\x08\x00\x0a\x0a\x12\x12\xe6\xdd\xe6\xe5\xf0\xee\xf0\xef\x03\x00\x31\x00\x20\x00\x12\x00\x14\x00\xff\xff\x85\x00" },
	[   416 ] = {   6, "\x04\x00\x00\x00\x0a\x00" },
	[   418 ] = {  44, "\x02\x00\x00\x00\x56\x46\x53\x20\x76\x65\x72\x20\x33\x2e\x37\x32\x44\x20\x76\x63\x33\x2d\x73\x79\x73\x2e\x72\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" },
	[   420 ] = {   4, "\x0e\x00\x00\x00" },
	[   423 ] = {   6, "\x05\x00\x00\x00\x08\x00" },
	[   425 ] = {   6, "\x05\x00\x00\x00\x08\x00" },
	[   427 ] = {   6, "\x05\x00\x00\x00\x08\x00" },
	[   429 ] = {   6, "\x04\x00\x00\x00\x00\x00" },
	[   431 ] = {   6, "\x04\x00\x00\x00\x08\x00" },
	[   433 ] = {   6, "\x05\x00\x00\x00\x32\x00" },
	[   435 ] = {   4, "\x03\x00\x00\x00" },
	[   438 ] = {   6, "\x04\x00\x00\x00\x00\x00" },
	[   440 ] = {   6, "\x04\x00\x00\x00\x00\x00" },
	[   442 ] = {   4, "\x0e\x00\x00\x00" },
	[   445 ] = {   6, "\x04\x00\x00\x00\x08\x00" },
	[   447 ] = {   6, "\x05\x00\x00\x00\x32\x00" },
	[   449 ] = {   4, "\x03\x00\x00\x00" },
	}
};
