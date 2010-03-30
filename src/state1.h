int S1_unchecked (struct vfs_dev *dev)
{
	_(  Peek (dev, 0x00001fe8, 0x04));
	_(  Peek (dev, 0x00001fec, 0x04));
	_(  Peek (dev, 0x00001ff0, 0x04));
	_(  Peek (dev, 0x00001ff4, 0x04));
	_(  Peek (dev, 0x00001ff8, 0x04));
	_(  Peek (dev, 0x00001ffc, 0x04));
	_(  GetParam (dev, 0x002e));
	_(  GetVersion (dev));
	_(  GetParam (dev, 0x0028));
	_(  GetParam (dev, 0x0014));
	_(  GetParam (dev, 0x0011));
	_(  GetParam (dev, 0x0054));
	_(  GetParam (dev, 0x0001));
	_(  GetParam (dev, 0x0014));
	_(  AbortPrint (dev));
	_(  LoadImage (dev));
	_(  GetPrint (dev, 0x0001, type_0));
	_(  LoadImage (dev));
	_(  AbortPrint (dev));
	_(  LoadImage (dev));
	_(  SetParam (dev, 0x0004, 0x0000));
	_(  SetParam (dev, 0x0005, 0x0000));
	_(  SetParam (dev, 0x0006, 0x0000));
	_(  SetParam (dev, 0x0007, 0x0000));
	_(  SetParam (dev, 0x000a, 0x0002));
	_(  SetParam (dev, 0x000b, 0x010b));
	_(  SetParam (dev, 0x000c, mess_with_bc));
	_(  SetParam (dev, 0x000d, 0x010d));
	_(  SetParam (dev, 0x000e, 0x0001));
	_(  SetParam (dev, 0x0010, 0x0000));
	_(  SetParam (dev, 0x0012, 0x0040));
	_(  SetParam (dev, 0x0015, 0x04b0));
	_(  SetParam (dev, 0x0016, 0x001b));
	_(  SetParam (dev, 0x0017, 0x0001));
	_(  SetParam (dev, 0x0018, 0x0003));
	_(  SetParam (dev, 0x0019, 0x0001));
	_(  SetParam (dev, 0x001a, 0x0000));
	_(  SetParam (dev, 0x001b, 0x0004));
	_(  SetParam (dev, 0x001d, 0x010c));
	_(  SetParam (dev, 0x001e, 0x010d));
	_(  SetParam (dev, 0x001f, 0x0000));
	_(  SetParam (dev, 0x0020, 0x0000));
	_(  SetParam (dev, 0x0021, 0x0001));
	_(  SetParam (dev, 0x0022, 0x0001));
	_(  SetParam (dev, 0x0023, 0x0001));
	_(  SetParam (dev, 0x0024, 0x0000));
	_(  SetParam (dev, 0x002c, 0x0046));
	_(  SetParam (dev, 0x002d, 0x002a));
	_(  SetParam (dev, 0x002f, 0x010e));
	_(  SetParam (dev, 0x0030, 0x010f));
	_(  SetParam (dev, 0x0032, 0x0004));
	_(  SetParam (dev, 0x0033, 0x0003));
	_(  SetParam (dev, 0x0034, 0x0005));
	_(  SetParam (dev, 0x0035, 0x0010));
	_(  SetParam (dev, 0x0036, 0x000a));
	_(  SetParam (dev, 0x0037, 0x0018));
	_(  SetParam (dev, 0x0038, 0x001e));
	_(  SetParam (dev, 0x0039, 0x00b2));
	_(  SetParam (dev, 0x003a, 0x0080));
	_(  SetParam (dev, 0x003b, 0x0000));
	_(  SetParam (dev, 0x003e, 0x01f4));
	_(  SetParam (dev, 0x003f, 0x001a));
	_(  SetParam (dev, 0x0040, 0x00f8));
	_(  SetParam (dev, 0x0042, 0x0001));
	_(  SetParam (dev, 0x0043, 0x0004));
	_(  SetParam (dev, 0x0044, 0x0010));
	_(  SetParam (dev, 0x0045, 0x0005));
	_(  SetParam (dev, 0x0046, 0x00f5));
	_(  SetParam (dev, 0x0047, 0x000c));
	_(  SetParam (dev, 0x0048, 0x0000));
	_(  SetParam (dev, 0x0049, 0x0000));
	_(  SetParam (dev, 0x0053, 0x0000));
	_(  SetParam (dev, 0x0056, 0x00b4));
	_(  SetParam (dev, 0x0057, 0x0096));
	_(  SetParam (dev, 0x0058, 0x008c));
	_(  SetParam (dev, 0x0059, 0x0064));
	_(  SetParam (dev, 0x005b, 0x0001));
	_(  SetParam (dev, 0x005c, 0x0001));
	_(  SetParam (dev, 0x005d, 0x0020));
	_(  SetParam (dev, 0x005e, 0x0064));
	_(  SetParam (dev, 0x005f, 0x00c8));
	_(  SetParam (dev, 0x0060, 0x00c8));
	_(  SetParam (dev, 0x0062, 0x0000));
	_(  SetParam (dev, 0x0064, 0x011a));
	_(  SetParam (dev, 0x0069, 0x0014));
	_(  GetParam (dev, 0x002a));
	_(  GetParam (dev, 0x003c));
	_(  GetParam (dev, 0x004a));
	_(  GetParam (dev, 0x0041));
	_(  GetVersion (dev));
	_(  SetParam (dev, 0x0063, 0x0001));
	_(  SensorSpiTrans (dev, 0x05, 0x00, 0xab, 0x00, 0x00, 0x00, 0x00));
	_(  SetParam (dev, 0x0064, 0x0118));
	_(  GetConfig (dev));
	_(  SetParam (dev, 0x0046, 0x00f5));
	_(  SetParam (dev, 0x006d, 0x0032));
	_(  SetParam (dev, 0x006e, 0x0003));
	_(  GetParam (dev, 0x0052));
	_(  SetParam (dev, 0x0052, 0x0320));
	_(  GetPrint (dev, 0x0001, type_2));
	_(  LoadImage (dev));
	_(  SetParam (dev, 0x0052, 0x1eb4));
	_(  Peek (dev, 0x00ff502c, 0x02));
	_(  Peek (dev, 0x00ff502e, 0x02));
	_(  Poke (dev, 0x000005f6, 0x00000001, 0x01));
	_(  Peek (dev, 0x00ff503e, 0x01));
	_(  Poke (dev, 0x00ff503e, 0x00000000, 0x01));
	_(  Peek (dev, 0x00ff9802, 0x01));
	_(  Peek (dev, 0x00ff9800, 0x01));
	_(  Peek (dev, 0x00ff9806, 0x01));
	_(  Poke (dev, 0x00ff9806, 0x00000000, 0x01));
	_(  GetPrint (dev, 0x0064, type_0));
	_(  LoadImage (dev));
	_(  Poke (dev, 0x000005f6, 0x00000000, 0x01));
	_(  Peek (dev, 0x00ff503e, 0x01));
	_(  Poke (dev, 0x00ff503e, 0x00000010, 0x01));
	_(  Peek (dev, 0x00ff9802, 0x01));
	_(  Peek (dev, 0x00ff9800, 0x01));
	_(  Peek (dev, 0x00ff9806, 0x01));
	_(  GetPrint (dev, 0x0064, type_0));
	_(  LoadImage (dev));
	_(  Peek (dev, 0x00ff5038, 0x01));
	_(  Peek (dev, 0x00ff500e, 0x02));
	_(  Peek (dev, 0x00ff5032, 0x01));
	_(  Poke (dev, 0x00ff5032, 0x00000012, 0x01));
	_(  Poke (dev, 0x00ff500e, 0x00004000, 0x02));
	_(  Poke (dev, 0x00ff5038, 0x0000000f, 0x01));
	_(  SetParam (dev, 0x0062, 0x0000));
	_(  SetParam (dev, 0x0077, 0x0000));
	_(  SetParam (dev, 0x0076, 0x0000));
	_(  SetParam (dev, 0x0078, 0x0000));
	_(  GetPrint (dev, 0x0002, type_0));
	_(  LoadImage (dev));
	_(  Poke (dev, 0x000005f6, 0x00000001, 0x01));
	_(  Peek (dev, 0x00ff503e, 0x01));
	_(  Poke (dev, 0x00ff503e, 0x00000000, 0x01));
	_(  Peek (dev, 0x00ff9802, 0x01));
	_(  Peek (dev, 0x00ff9800, 0x01));
	_(  Peek (dev, 0x00ff9806, 0x01));
	_(  scan_contrast (dev));
	_(  Poke (dev, 0x000005f6, 0x00000000, 0x01));
	_(  Peek (dev, 0x00ff503e, 0x01));
	_(  Poke (dev, 0x00ff503e, 0x00000010, 0x01));
	_(  Peek (dev, 0x00ff9802, 0x01));
	_(  Peek (dev, 0x00ff9800, 0x01));
	_(  Peek (dev, 0x00ff9806, 0x01));
	_(  Poke (dev, 0x00ff9806, 0x00000000, 0x01));
	_(  GetPrint (dev, 0x000a, type_0));
	_(  LoadImage (dev));
	_(  SetParam (dev, 0x0077, best_contrast));
	_(  SetParam (dev, 0x0076, 0x0012));
	_(  SetParam (dev, 0x0078, 0x2230));
	_(  Poke (dev, 0x00ff5038, 0x00000014, 0x01));
	_(  Poke (dev, 0x00ff500e, exposure, 0x02));
	_(  Poke (dev, 0x00ff5032, 0x00000031, 0x01));
	_(  SetParam (dev, 0x0062, info_line_rate));
	_(  AbortPrint (dev));
	_(  LoadImage (dev));
	_(  SetParam (dev, 0x0062, info_line_rate));
	_(  GetVersion (dev));
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
int S1_checked (struct vfs_dev *dev)
{
	__(    6,    Peek (dev, 0x00001fe8, 0x04));
	__(    8,    Peek (dev, 0x00001fec, 0x04));
	__(   10,    Peek (dev, 0x00001ff0, 0x04));
	__(   12,    Peek (dev, 0x00001ff4, 0x04));
	__(   14,    Peek (dev, 0x00001ff8, 0x04));
	__(   16,    Peek (dev, 0x00001ffc, 0x04));
	__(   18,    GetParam (dev, 0x002e));
	__(   20,    GetVersion (dev));
	__(   22,    GetParam (dev, 0x0028));
	__(   24,    GetParam (dev, 0x0014));
	__(   26,    GetParam (dev, 0x0011));
	__(   28,    GetParam (dev, 0x0054));
	__(   30,    GetParam (dev, 0x0001));
	__(   32,    GetParam (dev, 0x0014));
	__(   34,    AbortPrint (dev));
	 _(          LoadImage (dev));
	__(   37,    GetPrint (dev, 0x0001, type_0));
	 _(          LoadImage (dev));
	__(   40,    AbortPrint (dev));
	 _(          LoadImage (dev));
	__(   43,    SetParam (dev, 0x0004, 0x0000));
	__(   45,    SetParam (dev, 0x0005, 0x0000));
	__(   47,    SetParam (dev, 0x0006, 0x0000));
	__(   49,    SetParam (dev, 0x0007, 0x0000));
	__(   51,    SetParam (dev, 0x000a, 0x0002));
	__(   53,    SetParam (dev, 0x000b, 0x010b));
	__(   55,    SetParam (dev, 0x000c, mess_with_bc));
	__(   57,    SetParam (dev, 0x000d, 0x010d));
	__(   59,    SetParam (dev, 0x000e, 0x0001));
	__(   61,    SetParam (dev, 0x0010, 0x0000));
	__(   63,    SetParam (dev, 0x0012, 0x0040));
	__(   65,    SetParam (dev, 0x0015, 0x04b0));
	__(   67,    SetParam (dev, 0x0016, 0x001b));
	__(   69,    SetParam (dev, 0x0017, 0x0001));
	__(   71,    SetParam (dev, 0x0018, 0x0003));
	__(   73,    SetParam (dev, 0x0019, 0x0001));
	__(   75,    SetParam (dev, 0x001a, 0x0000));
	__(   77,    SetParam (dev, 0x001b, 0x0004));
	__(   79,    SetParam (dev, 0x001d, 0x010c));
	__(   81,    SetParam (dev, 0x001e, 0x010d));
	__(   83,    SetParam (dev, 0x001f, 0x0000));
	__(   85,    SetParam (dev, 0x0020, 0x0000));
	__(   87,    SetParam (dev, 0x0021, 0x0001));
	__(   89,    SetParam (dev, 0x0022, 0x0001));
	__(   91,    SetParam (dev, 0x0023, 0x0001));
	__(   93,    SetParam (dev, 0x0024, 0x0000));
	__(   95,    SetParam (dev, 0x002c, 0x0046));
	__(   97,    SetParam (dev, 0x002d, 0x002a));
	__(   99,    SetParam (dev, 0x002f, 0x010e));
	__(  101,    SetParam (dev, 0x0030, 0x010f));
	__(  103,    SetParam (dev, 0x0032, 0x0004));
	__(  105,    SetParam (dev, 0x0033, 0x0003));
	__(  107,    SetParam (dev, 0x0034, 0x0005));
	__(  109,    SetParam (dev, 0x0035, 0x0010));
	__(  111,    SetParam (dev, 0x0036, 0x000a));
	__(  113,    SetParam (dev, 0x0037, 0x0018));
	__(  115,    SetParam (dev, 0x0038, 0x001e));
	__(  117,    SetParam (dev, 0x0039, 0x00b2));
	__(  119,    SetParam (dev, 0x003a, 0x0080));
	__(  121,    SetParam (dev, 0x003b, 0x0000));
	__(  123,    SetParam (dev, 0x003e, 0x01f4));
	__(  125,    SetParam (dev, 0x003f, 0x001a));
	__(  127,    SetParam (dev, 0x0040, 0x00f8));
	__(  129,    SetParam (dev, 0x0042, 0x0001));
	__(  131,    SetParam (dev, 0x0043, 0x0004));
	__(  133,    SetParam (dev, 0x0044, 0x0010));
	__(  135,    SetParam (dev, 0x0045, 0x0005));
	__(  137,    SetParam (dev, 0x0046, 0x00f5));
	__(  139,    SetParam (dev, 0x0047, 0x000c));
	__(  141,    SetParam (dev, 0x0048, 0x0000));
	__(  143,    SetParam (dev, 0x0049, 0x0000));
	__(  145,    SetParam (dev, 0x0053, 0x0000));
	__(  147,    SetParam (dev, 0x0056, 0x00b4));
	__(  149,    SetParam (dev, 0x0057, 0x0096));
	__(  151,    SetParam (dev, 0x0058, 0x008c));
	__(  153,    SetParam (dev, 0x0059, 0x0064));
	__(  155,    SetParam (dev, 0x005b, 0x0001));
	__(  157,    SetParam (dev, 0x005c, 0x0001));
	__(  159,    SetParam (dev, 0x005d, 0x0020));
	__(  161,    SetParam (dev, 0x005e, 0x0064));
	__(  163,    SetParam (dev, 0x005f, 0x00c8));
	__(  165,    SetParam (dev, 0x0060, 0x00c8));
	__(  167,    SetParam (dev, 0x0062, 0x0000));
	__(  169,    SetParam (dev, 0x0064, 0x011a));
	__(  171,    SetParam (dev, 0x0069, 0x0014));
	__(  173,    GetParam (dev, 0x002a));
	__(  175,    GetParam (dev, 0x003c));
	__(  177,    GetParam (dev, 0x004a));
	__(  179,    GetParam (dev, 0x0041));
	__(  181,    GetVersion (dev));
	__(  183,    SetParam (dev, 0x0063, 0x0001));
	__(  185,    SensorSpiTrans (dev, 0x05, 0x00, 0xab, 0x00, 0x00, 0x00, 0x00));
	__(  187,    SetParam (dev, 0x0064, 0x0118));
	__(  189,    GetConfig (dev));
	__(  191,    SetParam (dev, 0x0046, 0x00f5));
	__(  193,    SetParam (dev, 0x006d, 0x0032));
	__(  195,    SetParam (dev, 0x006e, 0x0003));
	__(  197,    GetParam (dev, 0x0052));
	__(  199,    SetParam (dev, 0x0052, 0x0320));
	__(  201,    GetPrint (dev, 0x0001, type_2));
	 _(          LoadImage (dev));
	__(  204,    SetParam (dev, 0x0052, 0x1eb4));
	__(  206,    Peek (dev, 0x00ff502c, 0x02));
	__(  208,    Peek (dev, 0x00ff502e, 0x02));
	__(  210,    Poke (dev, 0x000005f6, 0x00000001, 0x01));
	__(  212,    Peek (dev, 0x00ff503e, 0x01));
	__(  214,    Poke (dev, 0x00ff503e, 0x00000000, 0x01));
	__(  216,    Peek (dev, 0x00ff9802, 0x01));
	__(  218,    Peek (dev, 0x00ff9800, 0x01));
	__(  220,    Peek (dev, 0x00ff9806, 0x01));
	__(  222,    Poke (dev, 0x00ff9806, 0x00000000, 0x01));
	__(  224,    GetPrint (dev, 0x0064, type_0));
	 _(          LoadImage (dev));
	__(  227,    Poke (dev, 0x000005f6, 0x00000000, 0x01));
	__(  229,    Peek (dev, 0x00ff503e, 0x01));
	__(  231,    Poke (dev, 0x00ff503e, 0x00000010, 0x01));
	__(  233,    Peek (dev, 0x00ff9802, 0x01));
	__(  235,    Peek (dev, 0x00ff9800, 0x01));
	__(  237,    Peek (dev, 0x00ff9806, 0x01));
	__(  239,    GetPrint (dev, 0x0064, type_0));
	 _(          LoadImage (dev));
	__(  242,    Peek (dev, 0x00ff5038, 0x01));
	__(  244,    Peek (dev, 0x00ff500e, 0x02));
	__(  246,    Peek (dev, 0x00ff5032, 0x01));
	__(  248,    Poke (dev, 0x00ff5032, 0x00000012, 0x01));
	__(  250,    Poke (dev, 0x00ff500e, 0x00004000, 0x02));
	__(  252,    Poke (dev, 0x00ff5038, 0x0000000f, 0x01));
	__(  254,    SetParam (dev, 0x0062, 0x0000));
	__(  256,    SetParam (dev, 0x0077, 0x0000));
	__(  258,    SetParam (dev, 0x0076, 0x0000));
	__(  260,    SetParam (dev, 0x0078, 0x0000));
	__(  262,    GetPrint (dev, 0x0002, type_0));
	 _(          LoadImage (dev));
	__(  265,    Poke (dev, 0x000005f6, 0x00000001, 0x01));
	__(  267,    Peek (dev, 0x00ff503e, 0x01));
	__(  269,    Poke (dev, 0x00ff503e, 0x00000000, 0x01));
	__(  271,    Peek (dev, 0x00ff9802, 0x01));
	__(  273,    Peek (dev, 0x00ff9800, 0x01));
	__(  275,    Peek (dev, 0x00ff9806, 0x01));
	 _(          scan_contrast (dev));
	__(  307,    Poke (dev, 0x000005f6, 0x00000000, 0x01));
	__(  309,    Peek (dev, 0x00ff503e, 0x01));
	__(  311,    Poke (dev, 0x00ff503e, 0x00000010, 0x01));
	__(  313,    Peek (dev, 0x00ff9802, 0x01));
	__(  315,    Peek (dev, 0x00ff9800, 0x01));
	__(  317,    Peek (dev, 0x00ff9806, 0x01));
	__(  319,    Poke (dev, 0x00ff9806, 0x00000000, 0x01));
	__(  321,    GetPrint (dev, 0x000a, type_0));
	 _(          LoadImage (dev));
	__(  324,    SetParam (dev, 0x0077, best_contrast));
	__(  326,    SetParam (dev, 0x0076, 0x0012));
	__(  328,    SetParam (dev, 0x0078, 0x2230));
	__(  330,    Poke (dev, 0x00ff5038, 0x00000014, 0x01));
	__(  332,    Poke (dev, 0x00ff500e, exposure, 0x02));
	__(  334,    Poke (dev, 0x00ff5032, 0x00000031, 0x01));
	__(  336,    SetParam (dev, 0x0062, info_line_rate));
	__(  338,    AbortPrint (dev));
	 _(          LoadImage (dev));
	__(  341,    SetParam (dev, 0x0062, info_line_rate));
	__(  343,    GetVersion (dev));
	__(  345,    SetParam (dev, 0x0055, 0x0008));
	__(  347,    GetParam (dev, 0x0014));
	__(  349,    GetParam (dev, 0x0011));
	__(  351,    SetParam (dev, 0x0062, info_line_rate));
	__(  353,    GetPrint (dev, 0x0014, type_0));
	 _(          LoadImage (dev));
	__(  356,    GetParam (dev, 0x0014));
	__(  358,    GetParam (dev, 0x0014));
	__(  360,    AbortPrint (dev));
	 _(          LoadImage (dev));
	__(  363,    GetParam (dev, 0x0011));
	__(  365,    SetParam (dev, 0x0062, info_line_rate));
	__(  367,    GetPrint (dev, 0x1388, type_1));
	return 0;
}
struct result_table S1_results =
{
	367,
	{
	[     6 ] = {   8, "\x12\x00\x00\x00\x00\x00\x00\x00" },
	[     8 ] = {   8, "\x12\x00\x00\x00\x00\x00\x57\x21" },
	[    10 ] = {   8, "\x12\x00\x00\x00\x9f\x29\x01\x00" },
	[    12 ] = {   8, "\x12\x00\x00\x00\xdb\xdb\xdb\xdb" },
	[    14 ] = {   8, "\x12\x00\x00\x00\x00\x00\x00\x00" },
	[    16 ] = {   8, "\x12\x00\x00\x00\x20\x49\xfb\xd4" },
	[    18 ] = {   6, "\x04\x00\x00\x00\x0a\x00" },
	[    20 ] = {  44, "\x02\x00\x00\x00\x56\x46\x53\x20\x76\x65\x72\x20\x33\x2e\x37\x32\x44\x20\x76\x63\x33\x2d\x73\x79\x73\x2e\x72\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" },
	[    22 ] = {   6, "\x04\x00\x00\x00\x00\x00" },
	[    24 ] = {   6, "\x04\x00\x00\x00\x00\x00" },
	[    26 ] = {   6, "\x04\x00\x00\x00\x08\x00" },
	[    28 ] = {   6, "\x04\x00\x00\x00\x04\x00" },
	[    30 ] = {   6, "\x04\x00\x00\x00\xf4\x01" },
	[    32 ] = {   6, "\x04\x00\x00\x00\x00\x00" },
	[    34 ] = {   4, "\x0e\x00\x00\x00" },
	[    37 ] = {   4, "\x03\x00\x00\x00" },
	[    40 ] = {   4, "\x0e\x00\x00\x00" },
	[    43 ] = {   6, "\x05\x00\x03\x00\x00\x00" },
	[    45 ] = {   6, "\x05\x00\x03\x00\x00\x00" },
	[    47 ] = {   6, "\x05\x00\x03\x00\x00\x00" },
	[    49 ] = {   6, "\x05\x00\x03\x00\x00\x00" },
	[    51 ] = {   6, "\x05\x00\x00\x00\x02\x00" },
	[    53 ] = {   6, "\x05\x00\x00\x00\x0b\x01" },
	[    55 ] = {   6, "\x05\x00\x00\x00\x0c\x01" },
	[    57 ] = {   6, "\x05\x00\x00\x00\x0d\x01" },
	[    59 ] = {   6, "\x05\x00\x00\x00\x01\x00" },
	[    61 ] = {   6, "\x05\x00\x00\x00\x00\x00" },
	[    63 ] = {   6, "\x05\x00\x03\x00\x00\x00" },
	[    65 ] = {   6, "\x05\x00\x00\x00\xb0\x04" },
	[    67 ] = {   6, "\x05\x00\x00\x00\x1b\x00" },
	[    69 ] = {   6, "\x05\x00\x00\x00\x01\x00" },
	[    71 ] = {   6, "\x05\x00\x03\x00\x01\x00" },
	[    73 ] = {   6, "\x05\x00\x00\x00\x01\x00" },
	[    75 ] = {   6, "\x05\x00\x00\x00\x00\x00" },
	[    77 ] = {   6, "\x05\x00\x00\x00\x04\x00" },
	[    79 ] = {   6, "\x05\x00\x00\x00\x0c\x01" },
	[    81 ] = {   6, "\x05\x00\x00\x00\x0d\x01" },
	[    83 ] = {   6, "\x05\x00\x03\x00\x0d\x01" },
	[    85 ] = {   6, "\x05\x00\x03\x00\x0d\x01" },
	[    87 ] = {   6, "\x05\x00\x03\x00\x0d\x01" },
	[    89 ] = {   6, "\x05\x00\x03\x00\x0d\x01" },
	[    91 ] = {   6, "\x05\x00\x03\x00\x0d\x01" },
	[    93 ] = {   6, "\x05\x00\x03\x00\x0d\x01" },
	[    95 ] = {   6, "\x05\x00\x00\x00\x46\x00" },
	[    97 ] = {   6, "\x05\x00\x03\x00\x46\x00" },
	[    99 ] = {   6, "\x05\x00\x00\x00\x0e\x01" },
	[   101 ] = {   6, "\x05\x00\x00\x00\x0f\x01" },
	[   103 ] = {   6, "\x05\x00\x00\x00\x04\x00" },
	[   105 ] = {   6, "\x05\x00\x00\x00\x03\x00" },
	[   107 ] = {   6, "\x05\x00\x03\x00\x03\x00" },
	[   109 ] = {   6, "\x05\x00\x03\x00\x03\x00" },
	[   111 ] = {   6, "\x05\x00\x03\x00\x03\x00" },
	[   113 ] = {   6, "\x05\x00\x03\x00\x03\x00" },
	[   115 ] = {   6, "\x05\x00\x03\x00\x03\x00" },
	[   117 ] = {   6, "\x05\x00\x03\x00\x03\x00" },
	[   119 ] = {   6, "\x05\x00\x03\x00\x03\x00" },
	[   121 ] = {   6, "\x05\x00\x03\x00\x03\x00" },
	[   123 ] = {   6, "\x05\x00\x03\x00\x03\x00" },
	[   125 ] = {   6, "\x05\x00\x03\x00\x03\x00" },
	[   127 ] = {   6, "\x05\x00\x03\x00\x03\x00" },
	[   129 ] = {   6, "\x05\x00\x00\x00\x01\x00" },
	[   131 ] = {   6, "\x05\x00\x03\x00\x01\x00" },
	[   133 ] = {   6, "\x05\x00\x00\x00\x10\x00" },
	[   135 ] = {   6, "\x05\x00\x00\x00\x05\x00" },
	[   137 ] = {   6, "\x05\x00\x00\x00\xf5\x00" },
	[   139 ] = {   6, "\x05\x00\x00\x00\x0c\x00" },
	[   141 ] = {   6, "\x05\x00\x00\x00\x00\x00" },
	[   143 ] = {   6, "\x05\x00\x00\x00\x00\x00" },
	[   145 ] = {   6, "\x05\x00\x00\x00\x00\x00" },
	[   147 ] = {   6, "\x05\x00\x00\x00\xb4\x00" },
	[   149 ] = {   6, "\x05\x00\x00\x00\x96\x00" },
	[   151 ] = {   6, "\x05\x00\x00\x00\x8c\x00" },
	[   153 ] = {   6, "\x05\x00\x00\x00\x64\x00" },
	[   155 ] = {   6, "\x05\x00\x00\x00\x01\x00" },
	[   157 ] = {   6, "\x05\x00\x00\x00\x01\x00" },
	[   159 ] = {   6, "\x05\x00\x00\x00\x20\x00" },
	[   161 ] = {   6, "\x05\x00\x00\x00\x64\x00" },
	[   163 ] = {   6, "\x05\x00\x00\x00\xc8\x00" },
	[   165 ] = {   6, "\x05\x00\x00\x00\xc8\x00" },
	[   167 ] = {   6, "\x05\x00\x00\x00\x00\x00" },
	[   169 ] = {   6, "\x05\x00\x00\x00\x1a\x01" },
	[   171 ] = {   6, "\x05\x00\x00\x00\x14\x00" },
	[   173 ] = {   6, "\x04\x00\x03\x00\x14\x00" },
	[   175 ] = {   6, "\x04\x00\x03\x00\x14\x00" },
	[   177 ] = {   6, "\x04\x00\x00\x00\x6f\x00" },
	[   179 ] = {   6, "\x04\x00\x03\x00\x6f\x00" },
	[   181 ] = {  44, "\x02\x00\x00\x00\x56\x46\x53\x20\x76\x65\x72\x20\x33\x2e\x37\x32\x44\x20\x76\x63\x33\x2d\x73\x79\x73\x2e\x72\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" },
	[   183 ] = {   6, "\x05\x00\x00\x00\x01\x00" },
	[   185 ] = {   9, "\x14\x00\x00\x00\x00\x00\x00\x00\x00" },
	[   187 ] = {   6, "\x05\x00\x00\x00\x18\x01" },
	[   189 ] = {  34, "\x06\x00\x00\x00\x00\x00\x08\x00\x0a\x0a\x11\x11\xe6\xdd\xe6\xe5\xf0\xee\xf0\xef\x03\x00\x31\x00\x20\x00\x12\x00\x14\x00\xff\xff\x85\x00" },
	[   191 ] = {   6, "\x05\x00\x00\x00\xf5\x00" },
	[   193 ] = {   6, "\x05\x00\x00\x00\x32\x00" },
	[   195 ] = {   6, "\x05\x00\x00\x00\x03\x00" },
	[   197 ] = {   6, "\x04\x00\x00\x00\xb4\x1e" },
	[   199 ] = {   6, "\x05\x00\x00\x00\x20\x03" },
	[   201 ] = {   4, "\x03\x00\x00\x00" },
	[   204 ] = {   6, "\x05\x00\x00\x00\xb4\x1e" },
	[   206 ] = {   8, "\x12\x00\x00\x00\xf0\xee\x00\x00" },
	[   208 ] = {   8, "\x12\x00\x00\x00\xf0\xf0\x00\x00" },
	[   210 ] = {   4, "\x13\x00\x00\x00" },
	[   212 ] = {   8, "\x12\x00\x00\x00\x00\x00\x00\x00" },
	[   214 ] = {   4, "\x13\x00\x00\x00" },
	[   216 ] = {   8, "\x12\x00\x00\x00\xfb\x00\x00\x00" },
	[   218 ] = {   8, "\x12\x00\x00\x00\x03\x00\x00\x00" },
	[   220 ] = {   8, "\x12\x00\x00\x00\x10\x00\x00\x00" },
	[   222 ] = {   4, "\x13\x00\x00\x00" },
	[   224 ] = {   4, "\x03\x00\x00\x00" },
	[   227 ] = {   4, "\x13\x00\x00\x00" },
	[   229 ] = {   8, "\x12\x00\x00\x00\x00\x00\x00\x00" },
	[   231 ] = {   4, "\x13\x00\x00\x00" },
	[   233 ] = {   8, "\x12\x00\x00\x00\xfb\x00\x00\x00" },
	[   235 ] = {   8, "\x12\x00\x00\x00\x03\x00\x00\x00" },
	[   237 ] = {   8, "\x12\x00\x00\x00\x00\x00\x00\x00" },
	[   239 ] = {   4, "\x03\x00\x00\x00" },
	[   242 ] = {   8, "\x12\x00\x00\x00\x14\x00\x00\x00" },
	[   244 ] = {   8, "\x12\x00\x00\x00\xbc\x21\x00\x00" },
	[   246 ] = {   8, "\x12\x00\x00\x00\x31\x00\x00\x00" },
	[   248 ] = {   4, "\x13\x00\x00\x00" },
	[   250 ] = {   4, "\x13\x00\x00\x00" },
	[   252 ] = {   4, "\x13\x00\x00\x00" },
	[   254 ] = {   6, "\x05\x00\x00\x00\x00\x00" },
	[   256 ] = {   6, "\x05\x00\x00\x00\x00\x00" },
	[   258 ] = {   6, "\x05\x00\x00\x00\x00\x00" },
	[   260 ] = {   6, "\x05\x00\x00\x00\x00\x00" },
	[   262 ] = {   4, "\x03\x00\x00\x00" },
	[   265 ] = {   4, "\x13\x00\x00\x00" },
	[   267 ] = {   8, "\x12\x00\x00\x00\x00\x00\x00\x00" },
	[   269 ] = {   4, "\x13\x00\x00\x00" },
	[   271 ] = {   8, "\x12\x00\x00\x00\xfb\x00\x00\x00" },
	[   273 ] = {   8, "\x12\x00\x00\x00\x03\x00\x00\x00" },
	[   275 ] = {   8, "\x12\x00\x00\x00\x10\x00\x00\x00" },
	[   307 ] = {   4, "\x13\x00\x00\x00" },
	[   309 ] = {   8, "\x12\x00\x00\x00\x00\x00\x00\x00" },
	[   311 ] = {   4, "\x13\x00\x00\x00" },
	[   313 ] = {   8, "\x12\x00\x00\x00\xfb\x00\x00\x00" },
	[   315 ] = {   8, "\x12\x00\x00\x00\x03\x00\x00\x00" },
	[   317 ] = {   8, "\x12\x00\x00\x00\x10\x00\x00\x00" },
	[   319 ] = {   4, "\x13\x00\x00\x00" },
	[   321 ] = {   4, "\x03\x00\x00\x00" },
	[   324 ] = {   6, "\x05\x00\x00\x00\x09\x00" },
	[   326 ] = {   6, "\x05\x00\x00\x00\x12\x00" },
	[   328 ] = {   6, "\x05\x00\x00\x00\x30\x22" },
	[   330 ] = {   4, "\x13\x00\x00\x00" },
	[   332 ] = {   4, "\x13\x00\x00\x00" },
	[   334 ] = {   4, "\x13\x00\x00\x00" },
	[   336 ] = {   6, "\x05\x00\x00\x00\x32\x00" },
	[   338 ] = {   4, "\x0e\x00\x00\x00" },
	[   341 ] = {   6, "\x05\x00\x00\x00\x32\x00" },
	[   343 ] = {  44, "\x02\x00\x00\x00\x56\x46\x53\x20\x76\x65\x72\x20\x33\x2e\x37\x32\x44\x20\x76\x63\x33\x2d\x73\x79\x73\x2e\x72\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" },
	[   345 ] = {   6, "\x05\x00\x00\x00\x08\x00" },
	[   347 ] = {   6, "\x04\x00\x00\x00\x00\x00" },
	[   349 ] = {   6, "\x04\x00\x00\x00\x08\x00" },
	[   351 ] = {   6, "\x05\x00\x00\x00\x32\x00" },
	[   353 ] = {   4, "\x03\x00\x00\x00" },
	[   356 ] = {   6, "\x04\x00\x00\x00\x00\x00" },
	[   358 ] = {   6, "\x04\x00\x00\x00\x00\x00" },
	[   360 ] = {   4, "\x0e\x00\x00\x00" },
	[   363 ] = {   6, "\x04\x00\x00\x00\x08\x00" },
	[   365 ] = {   6, "\x05\x00\x00\x00\x32\x00" },
	[   367 ] = {   4, "\x03\x00\x00\x00" },
	}
};
