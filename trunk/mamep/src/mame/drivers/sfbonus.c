/*
"CGA" Amcoe HW (c) 1999-2004 Amcoe

Notes:
- Some games requires an operator password to prevent players from changing the settings
  it defaults to 123456

- The games don't cope well with corrupt NVRAM, and may fail to boot completely(!)

- The code to handle the 'multple' reel layers is dubious.  rowscroll values are always used
  based on only one of the tilemaps displayed in that screen region.

- Inputs not done, Lamps not done

- Printer busy errors, Hopper timeout errors

 Thanks to Olivier Galibert for the handy bitswapper tool :-)

*/

#include "driver.h"
#include "cpu/z80/z80.h"
#include "sound/okim6295.h"

static bitmap_t *temp_reel_bitmap;
static tilemap *sfbonus_tilemap;
static tilemap *sfbonus_reel_tilemap;
static tilemap *sfbonus_reel2_tilemap;
static tilemap *sfbonus_reel3_tilemap;
static tilemap *sfbonus_reel4_tilemap;
static UINT8 *sfbonus_tilemap_ram;
static UINT8 *sfbonus_reel_ram;
static UINT8 *sfbonus_reel2_ram;
static UINT8 *sfbonus_reel3_ram;
static UINT8 *sfbonus_reel4_ram;
static UINT8* sfbonus_videoram;
static UINT8 *sfbonus_vregs;
static UINT8 *nvram;
static size_t nvram_size;

static TILE_GET_INFO( get_sfbonus_tile_info )
{
	int code = sfbonus_tilemap_ram[(tile_index*2)+0] | (sfbonus_tilemap_ram[(tile_index*2)+1]<<8);
	int flipx = (sfbonus_tilemap_ram[(tile_index*2)+1] & 0x80)>>7;
	int flipy = (sfbonus_tilemap_ram[(tile_index*2)+1] & 0x40)>>5;

	SET_TILE_INFO(
			0,
			code,
			0,
			TILE_FLIPYX(flipx | flipy));
}

static TILE_GET_INFO( get_sfbonus_reel_tile_info )
{
	int code = sfbonus_reel_ram[(tile_index*2)+0] | (sfbonus_reel_ram[(tile_index*2)+1]<<8);
	int flipx = (sfbonus_reel_ram[(tile_index*2)+1] & 0x80)>>7;
	int flipy = 0;//(sfbonus_reel_ram[(tile_index*2)+1] & 0x40)>>5;

	int priority = (sfbonus_reel_ram[(tile_index*2)+1] & 0x40)>>6;

	SET_TILE_INFO(
			1,
			code,
			priority,  // colour aboused as priority
			TILE_FLIPYX(flipx | flipy));
}

static TILE_GET_INFO( get_sfbonus_reel2_tile_info )
{
	int code = sfbonus_reel2_ram[(tile_index*2)+0] | (sfbonus_reel2_ram[(tile_index*2)+1]<<8);
	int flipx = (sfbonus_reel2_ram[(tile_index*2)+1] & 0x80)>>7;
	int flipy = 0;//(sfbonus_reel2_ram[(tile_index*2)+1] & 0x40)>>5;

	int priority = (sfbonus_reel2_ram[(tile_index*2)+1] & 0x40)>>6;

	SET_TILE_INFO(
			1,
			code,
			priority,  // colour abused as priority
			TILE_FLIPYX(flipx | flipy));
}

static TILE_GET_INFO( get_sfbonus_reel3_tile_info )
{
	int code = sfbonus_reel3_ram[(tile_index*2)+0] | (sfbonus_reel3_ram[(tile_index*2)+1]<<8);
	int flipx = (sfbonus_reel3_ram[(tile_index*2)+1] & 0x80)>>7;
	int flipy = 0;//(sfbonus_reel3_ram[(tile_index*2)+1] & 0x40)>>5;

	int priority = (sfbonus_reel3_ram[(tile_index*2)+1] & 0x40)>>6;

	SET_TILE_INFO(
			1,
			code,
			priority,  // colour abused as priority
			TILE_FLIPYX(flipx | flipy));
}

static TILE_GET_INFO( get_sfbonus_reel4_tile_info )
{
	int code = sfbonus_reel4_ram[(tile_index*2)+0] | (sfbonus_reel4_ram[(tile_index*2)+1]<<8);
	int flipx = (sfbonus_reel4_ram[(tile_index*2)+1] & 0x80)>>7;
	int flipy = 0;//(sfbonus_reel4_ram[(tile_index*2)+1] & 0x40)>>5;

	int priority = (sfbonus_reel4_ram[(tile_index*2)+1] & 0x40)>>6;

	SET_TILE_INFO(
			1,
			code,
			priority, // colour abused as priority
			TILE_FLIPYX(flipx | flipy));
}


static WRITE8_HANDLER( sfbonus_videoram_w )
{
	if (offset<0x4000) /* 0x0000 - 0x3fff */
	{
		sfbonus_tilemap_ram[offset] = data;
		tilemap_mark_tile_dirty(sfbonus_tilemap,offset/2);
	}
	else if (offset<0x4800) /* 0x4000 - 0x47ff */
	{
		offset-=0x4000;

		sfbonus_reel_ram[offset] = data;
		tilemap_mark_tile_dirty(sfbonus_reel_tilemap,offset/2);
	}
	else if (offset<0x5000)  /* 0x4800 - 0x4fff */
	{
		offset-=0x4800;

		sfbonus_reel2_ram[offset] = data;
		tilemap_mark_tile_dirty(sfbonus_reel2_tilemap,offset/2);
	}
	else if (offset<0x5800) /* 0x5000 - 0x57ff */
	{
		offset-=0x5000;

		sfbonus_reel3_ram[offset] = data;
		tilemap_mark_tile_dirty(sfbonus_reel3_tilemap,offset/2);
	}
	else if (offset<0x6000) /* 0x5800 - 0x5fff */
	{
		offset-=0x5800;

		sfbonus_reel4_ram[offset] = data;
		tilemap_mark_tile_dirty(sfbonus_reel4_tilemap,offset/2);
	}
	else if (offset<0x8000)
	{
		offset -=0x6000;
		// scroll regs etc.
		//logerror("access vram at [%04x] <- %02x\n",offset,data);
		sfbonus_videoram[offset] = data;
	}
	else
	{
		printf("access vram at %04x\n",offset);
	}

}



static VIDEO_START(sfbonus)
{
	temp_reel_bitmap = auto_bitmap_alloc(1024,512,BITMAP_FORMAT_INDEXED16);

	sfbonus_tilemap = tilemap_create(machine,get_sfbonus_tile_info,tilemap_scan_rows,8,8, 128, 64);
	sfbonus_reel_tilemap = tilemap_create(machine,get_sfbonus_reel_tile_info,tilemap_scan_rows,8,32, 64, 16);
	sfbonus_reel2_tilemap = tilemap_create(machine,get_sfbonus_reel2_tile_info,tilemap_scan_rows,8,32, 64, 16);
	sfbonus_reel3_tilemap = tilemap_create(machine,get_sfbonus_reel3_tile_info,tilemap_scan_rows,8,32, 64, 16);
	sfbonus_reel4_tilemap = tilemap_create(machine,get_sfbonus_reel4_tile_info,tilemap_scan_rows,8,32, 64, 16);

	tilemap_set_transparent_pen(sfbonus_tilemap,0);
	tilemap_set_transparent_pen(sfbonus_reel_tilemap,255);
	tilemap_set_transparent_pen(sfbonus_reel2_tilemap,255);
	tilemap_set_transparent_pen(sfbonus_reel3_tilemap,255);
	tilemap_set_transparent_pen(sfbonus_reel4_tilemap,255);

	tilemap_set_scroll_rows(sfbonus_tilemap,64);

	tilemap_set_scroll_cols(sfbonus_reel_tilemap, 64);
	tilemap_set_scroll_cols(sfbonus_reel2_tilemap, 64);
	tilemap_set_scroll_cols(sfbonus_reel3_tilemap, 64);
	tilemap_set_scroll_cols(sfbonus_reel4_tilemap, 64);


}

static void sfbonus_draw_reel_layer(running_machine* machine, bitmap_t *bitmap, const rectangle *cliprect, const device_config *screen, int catagory)
{
	int zz;
	int i;
	int startclipmin;
	const rectangle *visarea = video_screen_get_visible_area(screen);
	UINT8* selectbase = &sfbonus_videoram[0x600];
	UINT8* bg_scroll = &sfbonus_videoram[0x000];
	UINT8* reels_rowscroll = &sfbonus_videoram[0x400];
	int globalyscrollreels = (sfbonus_vregs[6] | sfbonus_vregs[7]<<8);
	int globalxscrollreels = (sfbonus_vregs[4] | sfbonus_vregs[5]<<8);
	globalyscrollreels += 8;
	globalxscrollreels += 8;

	startclipmin = 0;

	for (i= 0;i < 0x80;i++)
	{
		int scroll;
		scroll = bg_scroll[(i*2)+0x000] | (bg_scroll[(i*2)+0x001]<<8);
		tilemap_set_scrolly(sfbonus_reel_tilemap, i, scroll + globalyscrollreels );

		scroll = bg_scroll[(i*2)+0x080] | (bg_scroll[(i*2)+0x081]<<8);
		tilemap_set_scrolly(sfbonus_reel2_tilemap, i, scroll + globalyscrollreels);

		scroll = bg_scroll[(i*2)+0x100] | (bg_scroll[(i*2)+0x101]<<8);
		tilemap_set_scrolly(sfbonus_reel3_tilemap, i, scroll + globalyscrollreels);

		scroll = bg_scroll[(i*2)+0x180] | (bg_scroll[(i*2)+0x181]<<8);
		tilemap_set_scrolly(sfbonus_reel4_tilemap, i, scroll + globalyscrollreels);
	}

//  printf("------------\n");
	for (zz=0;zz<288;zz++)
	{
		rectangle clip;

		// other bits are used too..
		int line = ((zz+globalyscrollreels)&0x1ff);
		int rowenable = selectbase[line]&0x3;
		int rowenable2 = (selectbase[line]&0xc)>>2;
		int xxxscroll;
		int rowscroll;


		//printf("%04x %04x %d\n",zz, xxxscroll, line/8);

		/* draw top of screen */
		clip.min_x = visarea->min_x;
		clip.max_x = 511;
		clip.min_y = startclipmin;
		clip.max_y = startclipmin;



		// other bits are set, what do they mean?
		if (rowenable==0)
		{
			rowscroll = reels_rowscroll[((line/8)*2)+0x000] | (reels_rowscroll[((line/8)*2)+0x001]<<8);
			xxxscroll = globalxscrollreels + rowscroll;
			tilemap_set_scrollx(sfbonus_reel_tilemap, 0, xxxscroll  );
			tilemap_set_scrollx(sfbonus_reel2_tilemap, 0, xxxscroll );
			tilemap_set_scrollx(sfbonus_reel3_tilemap, 0, xxxscroll );
			tilemap_set_scrollx(sfbonus_reel4_tilemap, 0, xxxscroll );
		}
		else if (rowenable==0x1)
		{
			rowscroll = reels_rowscroll[((line/8)*2)+0x080] | (reels_rowscroll[((line/8)*2)+0x081]<<8);
			xxxscroll = globalxscrollreels + rowscroll;
			tilemap_set_scrollx(sfbonus_reel_tilemap, 0, xxxscroll  );
			tilemap_set_scrollx(sfbonus_reel2_tilemap, 0, xxxscroll );
			tilemap_set_scrollx(sfbonus_reel3_tilemap, 0, xxxscroll );
			tilemap_set_scrollx(sfbonus_reel4_tilemap, 0, xxxscroll );
		}
		else if (rowenable==0x2)
		{
			rowscroll = reels_rowscroll[((line/8)*2)+0x100] | (reels_rowscroll[((line/8)*2)+0x101]<<8);
			xxxscroll = globalxscrollreels + rowscroll;
			tilemap_set_scrollx(sfbonus_reel_tilemap, 0, xxxscroll  );
			tilemap_set_scrollx(sfbonus_reel2_tilemap, 0, xxxscroll );
			tilemap_set_scrollx(sfbonus_reel3_tilemap, 0, xxxscroll );
			tilemap_set_scrollx(sfbonus_reel4_tilemap, 0, xxxscroll );
		}
		else if (rowenable==0x3)
		{
			rowscroll = reels_rowscroll[((line/8)*2)+0x180] | (reels_rowscroll[((line/8)*2)+0x181]<<8);
			xxxscroll = globalxscrollreels + rowscroll;
			tilemap_set_scrollx(sfbonus_reel_tilemap, 0, xxxscroll  );
			tilemap_set_scrollx(sfbonus_reel2_tilemap, 0, xxxscroll );
			tilemap_set_scrollx(sfbonus_reel3_tilemap, 0, xxxscroll );
			tilemap_set_scrollx(sfbonus_reel4_tilemap, 0, xxxscroll );
		}

		if (rowenable2==0)
		{
			tilemap_draw(temp_reel_bitmap,&clip,sfbonus_reel_tilemap,TILEMAP_DRAW_CATEGORY(catagory),3);
		}
		if (rowenable==0)
		{
			tilemap_draw(temp_reel_bitmap,&clip,sfbonus_reel_tilemap,TILEMAP_DRAW_CATEGORY(catagory),3);
		}

		if (rowenable2==0x1)
		{
			tilemap_draw(temp_reel_bitmap,&clip,sfbonus_reel2_tilemap,TILEMAP_DRAW_CATEGORY(catagory),2);
		}
		if (rowenable==0x1)
		{
			tilemap_draw(temp_reel_bitmap,&clip,sfbonus_reel2_tilemap,TILEMAP_DRAW_CATEGORY(catagory),2);
		}

		if (rowenable2==0x2)
		{
			tilemap_draw(temp_reel_bitmap,&clip,sfbonus_reel3_tilemap,TILEMAP_DRAW_CATEGORY(catagory),1);
		}
		if (rowenable==0x2)
		{
			tilemap_draw(temp_reel_bitmap,&clip,sfbonus_reel3_tilemap,TILEMAP_DRAW_CATEGORY(catagory),1);
		}

		if (rowenable2==0x3)
		{
			tilemap_draw(temp_reel_bitmap,&clip,sfbonus_reel4_tilemap,TILEMAP_DRAW_CATEGORY(catagory),4);
		}
		if (rowenable==0x3)
		{
			tilemap_draw(temp_reel_bitmap,&clip,sfbonus_reel4_tilemap,TILEMAP_DRAW_CATEGORY(catagory),4);
		}





		startclipmin+=1;
	}

}

static VIDEO_UPDATE(sfbonus)
{

	int globalyscroll = (sfbonus_vregs[2] | sfbonus_vregs[3]<<8);
	int globalxscroll = (sfbonus_vregs[0] | sfbonus_vregs[1]<<8);
	UINT8* front_rowscroll = &sfbonus_videoram[0x200];
	int i;

	// align to 0
	globalyscroll += 8;
	globalxscroll += 8;

	bitmap_fill(bitmap,cliprect,screen->machine->pens[0]);
	bitmap_fill(temp_reel_bitmap,cliprect,screen->machine->pens[0]);

	/* render reels to bitmap */
	sfbonus_draw_reel_layer(screen->machine,temp_reel_bitmap,cliprect, screen, 0);

	{
		int y,x;

		for (y=0;y<288;y++)
		{
			for (x=0;x<512;x++)
			{
				UINT16* src = BITMAP_ADDR16(temp_reel_bitmap, y, x);
				UINT16* dst = BITMAP_ADDR16(bitmap, y, x);

				if ((src[0]&0x100)==0x000)
					dst[0] = src[0];
			}
		}
	}

	/* Normal Tilemap */
	tilemap_set_scrolly(sfbonus_tilemap, 0, globalyscroll );
	for (i=0;i<64;i++)
	{
		int scroll;
		scroll = front_rowscroll[(i*2)+0x000] | (front_rowscroll[(i*2)+0x001]<<8);
		tilemap_set_scrollx(sfbonus_tilemap, i, scroll+globalxscroll );
	}
	tilemap_draw(bitmap,cliprect,sfbonus_tilemap,0,0);

	{
		int y,x;

		for (y=0;y<288;y++)
		{
			for (x=0;x<512;x++)
			{
				UINT16* src = BITMAP_ADDR16(temp_reel_bitmap, y, x);
				UINT16* dst = BITMAP_ADDR16(bitmap, y, x);

				if ((src[0]&0x100)==0x100)
					dst[0] = src[0]-0x100;
			}
		}
	}

	/* High Priority Reels */
//  sfbonus_draw_reel_layer(screen->machine,bitmap,cliprect, screen, 1);

	return 0;
}


static WRITE8_HANDLER( paletteram_io_w )
{
	static int pal_offs,r,g,b,internal_pal_offs;

	switch(offset)
	{
		case 0:
			pal_offs = data;
			break;
		case 2:
			internal_pal_offs = 0;
			break;
		case 1:
			switch(internal_pal_offs)
			{
				case 0:
					r = ((data & 0x3f) << 2) | ((data & 0x30) >> 4);
					internal_pal_offs++;
					break;
				case 1:
					g = ((data & 0x3f) << 2) | ((data & 0x30) >> 4);
					internal_pal_offs++;
					break;
				case 2:
					b = ((data & 0x3f) << 2) | ((data & 0x30) >> 4);
					palette_set_color(space->machine, pal_offs, MAKE_RGB(r, g, b));
					internal_pal_offs = 0;
					pal_offs++;
					break;
			}

			break;
	}
}



static ADDRESS_MAP_START( sfbonus_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0xefff) AM_ROMBANK(1) AM_WRITE(sfbonus_videoram_w)
	AM_RANGE(0xf000, 0xffff) AM_RAM AM_BASE(&nvram) AM_SIZE(&nvram_size)
ADDRESS_MAP_END

static WRITE8_HANDLER( sfbonus_bank_w )
{
	UINT8 *ROM = memory_region(space->machine, "maincpu");
	UINT8 bank;

	bank = data & 7;

	memory_set_bankptr(space->machine, 1, &ROM[bank * 0x10000]);
}



static READ8_HANDLER( sfbonus_2800_r )
{
	return mame_rand(space->machine);
}

static READ8_HANDLER( sfbonus_2801_r )
{
	return mame_rand(space->machine);
}

static READ8_HANDLER( sfbonus_2c00_r )
{
	return mame_rand(space->machine);
}

static READ8_HANDLER( sfbonus_2c01_r )
{
	return mame_rand(space->machine);
}

static READ8_HANDLER( sfbonus_3800_r )
{
	return 0xff;
}


static ADDRESS_MAP_START( sfbonus_io, ADDRESS_SPACE_IO, 8 )
	AM_RANGE(0x0400, 0x0400) AM_READ_PORT("KEY1")
	AM_RANGE(0x0408, 0x0408) AM_READ_PORT("KEY2")
	AM_RANGE(0x0410, 0x0410) AM_READ_PORT("KEY3")

	AM_RANGE(0x0418, 0x0418) AM_READ_PORT("SWITCH1")
	AM_RANGE(0x0420, 0x0420) AM_READ_PORT("SWITCH2")
	AM_RANGE(0x0428, 0x0428) AM_READ_PORT("SWITCH3")
	AM_RANGE(0x0430, 0x0430) AM_READ_PORT("SWITCH4")
	AM_RANGE(0x0438, 0x0438) AM_READ_PORT("SWITCH5")

	AM_RANGE(0x0800, 0x0800) AM_DEVREADWRITE("oki", okim6295_r, okim6295_w)

	AM_RANGE(0x0c00, 0x0c03) AM_WRITE( paletteram_io_w )

	AM_RANGE(0x2400, 0x241f) AM_RAM AM_BASE(&sfbonus_vregs)

	AM_RANGE(0x2800, 0x2800) AM_READ(sfbonus_2800_r)
	AM_RANGE(0x2801, 0x2801) AM_READ(sfbonus_2801_r) AM_WRITE(SMH_NOP)

	AM_RANGE(0x2c00, 0x2c00) AM_READ(sfbonus_2c00_r)
	AM_RANGE(0x2c01, 0x2c01) AM_READ(sfbonus_2c01_r) AM_WRITE(SMH_NOP)

	AM_RANGE(0x3000, 0x3000) AM_WRITE(SMH_NOP)
	AM_RANGE(0x3400, 0x3400) AM_WRITE(sfbonus_bank_w)
	AM_RANGE(0x3800, 0x3800) AM_READ(sfbonus_3800_r) AM_WRITE(SMH_NOP)

	AM_RANGE(0x1800, 0x1800) AM_WRITE(SMH_NOP)
	AM_RANGE(0x1801, 0x1801) AM_WRITE(SMH_NOP)
	AM_RANGE(0x1802, 0x1802) AM_WRITE(SMH_NOP)
	AM_RANGE(0x1803, 0x1803) AM_WRITE(SMH_NOP)
	AM_RANGE(0x1804, 0x1804) AM_WRITE(SMH_NOP)
	AM_RANGE(0x1805, 0x1805) AM_WRITE(SMH_NOP)
	AM_RANGE(0x1806, 0x1806) AM_WRITE(SMH_NOP)

	AM_RANGE(0x3801, 0x3801) AM_WRITE(SMH_NOP)
	AM_RANGE(0x3802, 0x3802) AM_WRITE(SMH_NOP)
	AM_RANGE(0x3803, 0x3803) AM_WRITE(SMH_NOP)
	AM_RANGE(0x3806, 0x3806) AM_WRITE(SMH_NOP)
	AM_RANGE(0x3807, 0x3807) AM_WRITE(SMH_NOP)
ADDRESS_MAP_END

/* 8-liners input define */
static INPUT_PORTS_START( amcoebase )
	PORT_START("KEY1")
	PORT_DIPNAME( 0x01, 0x01, "Key1-0" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Key1-1" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Key1-2" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Key1-3" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Key1-4" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Key1-5" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Key1-6" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Key1-7" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START("KEY2")
	PORT_DIPNAME( 0x01, 0x01, "Key2-0" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Key2-1" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Key2-2" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Key2-3" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Key2-4" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Key2-5" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Key2-6" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Key2-7" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START("KEY3")
	PORT_DIPNAME( 0x01, 0x01, "Key3-0" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Key3-1" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Key3-2" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Key3-3" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Key3-4" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Key3-5" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Key3-6" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Key3-7" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	/* the dipswitches are probably for debugging the games only, all settings are in NVRAM */
	PORT_START("SWITCH1")
	PORT_DIPNAME( 0x01, 0x01, "Switch1-0" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Switch1-1" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Switch1-2" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Switch1-3" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Switch1-4" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Switch1-5" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Switch1-6" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Switch1-7" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START("SWITCH2")
	PORT_DIPNAME( 0x01, 0x01, "Switch2-0" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Switch2-1" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Switch2-2" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Switch2-3" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Switch2-4" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Switch2-5" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Switch2-6" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Switch2-7" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START("SWITCH3")
	PORT_DIPNAME( 0x01, 0x01, "Switch3-0" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Switch3-1" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Switch3-2" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Switch3-3" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Switch3-4" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Switch3-5" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Switch3-6" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Switch3-7" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START("SWITCH4")
	PORT_DIPNAME( 0x01, 0x01, "Switch4-0" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Switch4-1" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Switch4-2" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Switch4-3" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Switch4-4" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Switch4-5" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Switch4-6" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Switch4-7" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START("SWITCH5")
	PORT_DIPNAME( 0x01, 0x01, "Switch5-0" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Switch5-1" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Switch5-2" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "Switch5-3" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Switch5-4" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Switch5-5" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Switch5-6" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Switch5-7" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

static INPUT_PORTS_START( amcoecommon )
	PORT_INCLUDE(amcoebase)
	PORT_MODIFY("KEY1")
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON9)  PORT_NAME("Payout?") PORT_CODE(KEYCODE_Y) // causes hopper error
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON10) PORT_NAME("Clear SW.") PORT_CODE(KEYCODE_U)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON11) PORT_NAME("Account") PORT_CODE(KEYCODE_I)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON12) PORT_NAME("Confirm / Port Test") PORT_CODE(KEYCODE_O)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2)    PORT_IMPULSE(2) // causes coin jam if held
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN3)    // 'key in'

	PORT_MODIFY("KEY3")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1)
INPUT_PORTS_END

static INPUT_PORTS_START( amcoetype1 )
	PORT_INCLUDE(amcoecommon)

	PORT_MODIFY("KEY2")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SERVICE1)  PORT_IMPULSE(2) // causes service jam if held
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN1)     PORT_IMPULSE(2) // causes coin jam if held
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_NAME("All Stop / Left") PORT_CODE(KEYCODE_Z)
	/*        0x08 ? */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_NAME("Stop 1 / Select Letter / Double / Hold Help") PORT_CODE(KEYCODE_X)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON4 ) PORT_NAME("Stop 3 / Erase / Take") PORT_CODE(KEYCODE_V)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON5 ) PORT_NAME("Play / Bet") PORT_CODE(KEYCODE_B)
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_NAME("Stop 2 / Right") PORT_CODE(KEYCODE_C)
INPUT_PORTS_END

static INPUT_PORTS_START( amcoetype2 )
	PORT_INCLUDE(amcoecommon)

	PORT_MODIFY("KEY2") // reverse order
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_NAME("Small / Right") PORT_CODE(KEYCODE_C)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON5 ) PORT_NAME("Play / Bet") PORT_CODE(KEYCODE_B)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON4 ) PORT_NAME("Take") PORT_CODE(KEYCODE_V)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_NAME("Double / Select") PORT_CODE(KEYCODE_X)
	/*        0x10 ? */
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON4 ) PORT_NAME("Big / Left") PORT_CODE(KEYCODE_Z)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1)     PORT_IMPULSE(2) // causes coin jam if held
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1)  PORT_IMPULSE(2) // causes service jam if held
INPUT_PORTS_END


static const gfx_layout sfbonus_layout =
{
	8,8,
	RGN_FRAC(1,1),
	8,
	{ 0,1,2,3,4,5,6,7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	{ 0*64,1*64,2*64,3*64,4*64,5*64,6*64,7*64 },
	8*64
};

static const gfx_layout sfbonus32_layout =
{
	8,32,
	RGN_FRAC(1,1),
	8,
	{ 0,1,2,3,4,5,6,7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	{ 0*64,1*64,2*64,3*64,4*64,5*64,6*64,7*64,
      8*64,9*64,10*64,11*64,12*64,13*64,14*64,15*64,
	  16*64,17*64,18*64,19*64,20*64,21*64,22*64,23*64,
	  24*64,25*64,26*64,27*64,28*64,29*64,30*64,31*64
	  },
	32*64
};



static GFXDECODE_START( sfbonus )
	GFXDECODE_ENTRY( "gfx1", 0, sfbonus_layout,   0x0, 2  )
	GFXDECODE_ENTRY( "gfx2", 0, sfbonus32_layout,   0x0, 2  )
GFXDECODE_END


static MACHINE_RESET( sfbonus )
{
	UINT8 *ROM = memory_region(machine, "maincpu");

	memory_set_bankptr(machine, 1, &ROM[0]);
}

static NVRAM_HANDLER( sfbonus )
{
	if (read_or_write)
		mame_fwrite(file,nvram,nvram_size);
	else
	{
		if (file)
			mame_fread(file,nvram,nvram_size);
		else
			memset(nvram,0x00,nvram_size);
	}
}


static MACHINE_DRIVER_START( sfbonus )
	MDRV_CPU_ADD("maincpu", Z80, 6000000) // custom packaged z80 CPU ?? Mhz
	MDRV_CPU_PROGRAM_MAP(0,sfbonus_map)
	MDRV_CPU_IO_MAP(0,sfbonus_io)
	MDRV_CPU_VBLANK_INT("screen",irq0_line_hold)
//  MDRV_CPU_PERIODIC_INT(nmi_line_pulse,100)

	MDRV_MACHINE_RESET( sfbonus )

	MDRV_NVRAM_HANDLER(sfbonus)


	MDRV_GFXDECODE(sfbonus)

	MDRV_SCREEN_ADD("screen", RASTER)
	MDRV_SCREEN_REFRESH_RATE(60)
	MDRV_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(0))
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_SIZE(128*8, 64*8)
	MDRV_SCREEN_VISIBLE_AREA(0*8, 512-1, 0*8, 288-1)

	MDRV_PALETTE_LENGTH(0x100*2) // *2 for priority workaraound / custom drawing

	MDRV_VIDEO_START(sfbonus)
	MDRV_VIDEO_UPDATE(sfbonus)

	/* Parrot 3 seems fine at 1 Mhz, but Double Challenge isn't? */
	MDRV_SPEAKER_STANDARD_MONO("mono")
	MDRV_SOUND_ADD("oki", OKIM6295, 1000000)
	MDRV_SOUND_CONFIG(okim6295_interface_pin7high) // clock frequency & pin 7 not verified
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 1.00)
MACHINE_DRIVER_END


ROM_START( sfbonus )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80  Code */
	ROM_LOAD( "skfb16.bin", 0x00000, 0x40000, CRC(bfd53646) SHA1(bd58f8c6d5386649a6fc0f4bac46d1b6cd6248b1) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "skfbrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "skfbrom3.bin", 0x00000, 0x80000, CRC(36119517) SHA1(241bb256ab3ba595dcb0c81fd2e60ed35dd7c197) )
	ROM_LOAD16_BYTE( "skfbrom4.bin", 0x00001, 0x80000, CRC(a655bac1) SHA1(0faea01c09409f9182f08370dcc0b466a799f17f) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "skfbrom5.bin", 0x00000, 0x80000, CRC(752e6e3b) SHA1(46c3a1bbbf1a2afe36fa5333b6e74459e17e9bae) )
	ROM_LOAD16_BYTE( "skfbrom6.bin", 0x00001, 0x80000, CRC(30df6b6a) SHA1(7a180fa8ee64b9efb0321baffad72f0a9485d568) )
ROM_END

ROM_START( sfbonusa )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80  Code */
	ROM_LOAD( "skfb17.bin", 0x00000, 0x40000, CRC(e28ede82) SHA1(f320c4c9c30ec280ee2437d1ad4d2b6270580916) )

	ROM_REGION( 0x010000, "debugram", ROMREGION_ERASE00 ) /* DEBUG */


	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "skfbrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "skfbrom3.bin", 0x00000, 0x80000, CRC(36119517) SHA1(241bb256ab3ba595dcb0c81fd2e60ed35dd7c197) )
	ROM_LOAD16_BYTE( "skfbrom4.bin", 0x00001, 0x80000, CRC(a655bac1) SHA1(0faea01c09409f9182f08370dcc0b466a799f17f) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "skfbrom5a.bin", 0x00000, 0x80000, CRC(ed07a635) SHA1(a31ee06d8cb78c43affa9d899e9cec87a0875934) )
	ROM_LOAD16_BYTE( "skfbrom6a.bin", 0x00001, 0x80000, CRC(b9a39e5a) SHA1(cb875db25d8bccf48542bd6dbecc474b1205563b) )
ROM_END

ROM_START( sfbonusb )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80  Code */
	ROM_LOAD( "skfbb19r.bin", 0x00000, 0x40000, CRC(e185c0b7) SHA1(241aa3dc65f4399c465e43c5f7079f66f9998f01) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "skfbrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "skfbrom3.bin", 0x00000, 0x80000, CRC(36119517) SHA1(241bb256ab3ba595dcb0c81fd2e60ed35dd7c197) )
	ROM_LOAD16_BYTE( "skfbrom4.bin", 0x00001, 0x80000, CRC(a655bac1) SHA1(0faea01c09409f9182f08370dcc0b466a799f17f) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "skfbrom5a.bin", 0x00000, 0x80000, CRC(ed07a635) SHA1(a31ee06d8cb78c43affa9d899e9cec87a0875934) )
	ROM_LOAD16_BYTE( "skfbrom6a.bin", 0x00001, 0x80000, CRC(b9a39e5a) SHA1(cb875db25d8bccf48542bd6dbecc474b1205563b) )
ROM_END

ROM_START( sfbonusd )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80  Code */
	ROM_LOAD( "skfbd19r.bin", 0x00000, 0x40000, CRC(9e189177) SHA1(bb48053c516d036a1d18713d45a186a994a4c685) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "skfbrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "skfbrom3.bin", 0x00000, 0x80000, CRC(36119517) SHA1(241bb256ab3ba595dcb0c81fd2e60ed35dd7c197) )
	ROM_LOAD16_BYTE( "skfbrom4.bin", 0x00001, 0x80000, CRC(a655bac1) SHA1(0faea01c09409f9182f08370dcc0b466a799f17f) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "skfbrom5a.bin", 0x00000, 0x80000, CRC(ed07a635) SHA1(a31ee06d8cb78c43affa9d899e9cec87a0875934) )
	ROM_LOAD16_BYTE( "skfbrom6a.bin", 0x00001, 0x80000, CRC(b9a39e5a) SHA1(cb875db25d8bccf48542bd6dbecc474b1205563b) )
ROM_END

ROM_START( sfbonusv )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80  Code */
	ROM_LOAD( "skfbv19r.bin", 0x00000, 0x40000, CRC(f032be45) SHA1(63007ee7de6203ed7bda34e127328d085df20369) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "skfbrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "skfbrom3.bin", 0x00000, 0x80000, CRC(36119517) SHA1(241bb256ab3ba595dcb0c81fd2e60ed35dd7c197) )
	ROM_LOAD16_BYTE( "skfbrom4.bin", 0x00001, 0x80000, CRC(a655bac1) SHA1(0faea01c09409f9182f08370dcc0b466a799f17f) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "skfbrom5a.bin", 0x00000, 0x80000, CRC(ed07a635) SHA1(a31ee06d8cb78c43affa9d899e9cec87a0875934) )
	ROM_LOAD16_BYTE( "skfbrom6a.bin", 0x00001, 0x80000, CRC(b9a39e5a) SHA1(cb875db25d8bccf48542bd6dbecc474b1205563b) )
ROM_END


ROM_START( parrot3 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "p4p24.bin", 0x00000, 0x40000, CRC(356a49c8) SHA1(7e0ed7d1063675b66bfe28c427712249654be6ab) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "p4rom2.bin", 0x00000, 0x40000, CRC(d0574efc) SHA1(dd6628450883f0f723744e7caf6525bca7b18a43) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "p4rom3_24.bin", 0x00000, 0x80000, CRC(c5fc21cb) SHA1(b4137a97611ff688fbfa688eb3108622bed8da5b) )
	ROM_LOAD16_BYTE( "p4rom4_24.bin", 0x00001, 0x80000, CRC(bbe174d3) SHA1(75d964d37470843962419ead170f1db9a1dcc4c4) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "p4rom5_24.bin", 0x00000, 0x80000, CRC(5e184b6e) SHA1(a00eb5a62246ec00e1af6e8c0629a118f71f0c58) )
	ROM_LOAD16_BYTE( "p4rom6_24.bin", 0x00001, 0x80000, CRC(598d2117) SHA1(8391054aa8deb8480a69de97b8f5316e7864ed2d) )
ROM_END

ROM_START( parrot3b )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "p4pb26r.bin", 0x00000, 0x40000, CRC(c23202ec) SHA1(49d6f996cb32a2d16f6475bd55a755e3f9ed0fe7) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "p4rom2.bin", 0x00000, 0x40000, CRC(d0574efc) SHA1(dd6628450883f0f723744e7caf6525bca7b18a43) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "p4rom3.bin", 0x00000, 0x80000, CRC(2701d7ab) SHA1(9efeaa3dab2aa3f20501876db2100eae4a5b8af1) )
	ROM_LOAD16_BYTE( "p4rom4.bin", 0x00001, 0x80000, CRC(46ebe619) SHA1(811c2d35e4e04e8ecd7f4f2a7040de302d2ed91c) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "p4rom5.bin", 0x00000, 0x80000, CRC(34a00b25) SHA1(a0bf3b6a40b73e69d790d0f36d12de4851411995) )
	ROM_LOAD16_BYTE( "p4rom6.bin", 0x00001, 0x80000, CRC(4b9f30a6) SHA1(c6aac500085225d1684533dc765c6c5461a7e652) )
ROM_END

ROM_START( parrot3d )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "p4pd26r.bin", 0x00000, 0x40000, CRC(f68a623c) SHA1(d2166364d4ade9c3cc5c4dfd0331b69de35ec011) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "p4rom2.bin", 0x00000, 0x40000, CRC(d0574efc) SHA1(dd6628450883f0f723744e7caf6525bca7b18a43) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "p4rom3.bin", 0x00000, 0x80000, CRC(2701d7ab) SHA1(9efeaa3dab2aa3f20501876db2100eae4a5b8af1) )
	ROM_LOAD16_BYTE( "p4rom4.bin", 0x00001, 0x80000, CRC(46ebe619) SHA1(811c2d35e4e04e8ecd7f4f2a7040de302d2ed91c) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "p4rom5.bin", 0x00000, 0x80000, CRC(34a00b25) SHA1(a0bf3b6a40b73e69d790d0f36d12de4851411995) )
	ROM_LOAD16_BYTE( "p4rom6.bin", 0x00001, 0x80000, CRC(4b9f30a6) SHA1(c6aac500085225d1684533dc765c6c5461a7e652) )
ROM_END

ROM_START( parrot3v )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "p4pv26e.bin", 0x00000, 0x40000, CRC(d9a7be80) SHA1(71dfc333ed9e0e89439cf0970cec66a5a30da1cd) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "p4rom2.bin", 0x00000, 0x40000, CRC(d0574efc) SHA1(dd6628450883f0f723744e7caf6525bca7b18a43) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "p4rom3.bin", 0x00000, 0x80000, CRC(2701d7ab) SHA1(9efeaa3dab2aa3f20501876db2100eae4a5b8af1) )
	ROM_LOAD16_BYTE( "p4rom4.bin", 0x00001, 0x80000, CRC(46ebe619) SHA1(811c2d35e4e04e8ecd7f4f2a7040de302d2ed91c) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "p4rom5.bin", 0x00000, 0x80000, CRC(34a00b25) SHA1(a0bf3b6a40b73e69d790d0f36d12de4851411995) )
	ROM_LOAD16_BYTE( "p4rom6.bin", 0x00001, 0x80000, CRC(4b9f30a6) SHA1(c6aac500085225d1684533dc765c6c5461a7e652) )
ROM_END

ROM_START( parrot3v2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "p4pv26r.bin", 0x00000, 0x40000, CRC(f4f43a29) SHA1(b5f1eb40a6ffe1a1cc7df2f583b6fc0cfef2e703) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "p4rom2.bin", 0x00000, 0x40000, CRC(d0574efc) SHA1(dd6628450883f0f723744e7caf6525bca7b18a43) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "p4rom3.bin", 0x00000, 0x80000, CRC(2701d7ab) SHA1(9efeaa3dab2aa3f20501876db2100eae4a5b8af1) )
	ROM_LOAD16_BYTE( "p4rom4.bin", 0x00001, 0x80000, CRC(46ebe619) SHA1(811c2d35e4e04e8ecd7f4f2a7040de302d2ed91c) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "p4rom5.bin", 0x00000, 0x80000, CRC(34a00b25) SHA1(a0bf3b6a40b73e69d790d0f36d12de4851411995) )
	ROM_LOAD16_BYTE( "p4rom6.bin", 0x00001, 0x80000, CRC(4b9f30a6) SHA1(c6aac500085225d1684533dc765c6c5461a7e652) )
ROM_END

ROM_START( hldspin1 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "hs1p25t.bin", 0x00000, 0x40000, CRC(0fce5691) SHA1(4920ee490fdd690987bee92525b48596a051f83d) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "hs_2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "hs1_3.bin", 0x00000, 0x40000, CRC(85a016cb) SHA1(abb32c0191a531706593088b2ecfb48ceb02a127)  )
	ROM_LOAD16_BYTE( "hs1_4.bin", 0x00001, 0x40000, CRC(4313c099) SHA1(620452ac607b044ce4c8a5a7b03bc831125c81eb) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "hs_5.bin", 0x00000, 0x40000, CRC(09931910) SHA1(cac792f7c67d0ea274ecb369cef0554a033e8d88) )
	ROM_LOAD16_BYTE( "hs_6.bin", 0x00001, 0x40000, CRC(801703e4) SHA1(7da822b03a6d4f53a49bb1fedc9e1262d8a84782) )
ROM_END

ROM_START( hldspin1b )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "hs1b27t.bin", 0x00000, 0x40000, CRC(b4928a82) SHA1(c5521eb51887525fd6850ac36d148d3206db5493) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "hs_2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "hs1_3.bin", 0x00000, 0x40000, CRC(85a016cb) SHA1(abb32c0191a531706593088b2ecfb48ceb02a127)  )
	ROM_LOAD16_BYTE( "hs1_4.bin", 0x00001, 0x40000, CRC(4313c099) SHA1(620452ac607b044ce4c8a5a7b03bc831125c81eb) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "hs_5.bin", 0x00000, 0x40000, CRC(09931910) SHA1(cac792f7c67d0ea274ecb369cef0554a033e8d88) )
	ROM_LOAD16_BYTE( "hs_6.bin", 0x00001, 0x40000, CRC(801703e4) SHA1(7da822b03a6d4f53a49bb1fedc9e1262d8a84782) )
ROM_END

ROM_START( hldspin1d )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "hs1d27t.bin", 0x00000, 0x40000, CRC(c3fc35a3) SHA1(59a02815e004738f5eee43dffbeaca34412da308) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "hs_2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "hs1_3.bin", 0x00000, 0x40000, CRC(85a016cb) SHA1(abb32c0191a531706593088b2ecfb48ceb02a127)  )
	ROM_LOAD16_BYTE( "hs1_4.bin", 0x00001, 0x40000, CRC(4313c099) SHA1(620452ac607b044ce4c8a5a7b03bc831125c81eb) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "hs_5.bin", 0x00000, 0x40000, CRC(09931910) SHA1(cac792f7c67d0ea274ecb369cef0554a033e8d88) )
	ROM_LOAD16_BYTE( "hs_6.bin", 0x00001, 0x40000, CRC(801703e4) SHA1(7da822b03a6d4f53a49bb1fedc9e1262d8a84782) )
ROM_END

ROM_START( hldspin1v )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "hs1v27t.bin", 0x00000, 0x40000, CRC(99347659) SHA1(f8af779046e93a2514dc59b11bb8d7a11487b08e) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "hs_2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "hs1_3.bin", 0x00000, 0x40000, CRC(85a016cb) SHA1(abb32c0191a531706593088b2ecfb48ceb02a127)  )
	ROM_LOAD16_BYTE( "hs1_4.bin", 0x00001, 0x40000, CRC(4313c099) SHA1(620452ac607b044ce4c8a5a7b03bc831125c81eb) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "hs_5.bin", 0x00000, 0x40000, CRC(09931910) SHA1(cac792f7c67d0ea274ecb369cef0554a033e8d88) )
	ROM_LOAD16_BYTE( "hs_6.bin", 0x00001, 0x40000, CRC(801703e4) SHA1(7da822b03a6d4f53a49bb1fedc9e1262d8a84782) )
ROM_END

ROM_START( hldspin2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "hs2p26.bin", 0x00000, 0x40000, CRC(35844d85) SHA1(cd9bd3a95d1aaf4171bc9c57dec45b59fcc11902) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "hs_2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "hs2_3.bin", 0x00000, 0x40000, CRC(b6890061) SHA1(c196f8740d8487b108cff58d77a203b2d8431a67) )
	ROM_LOAD16_BYTE( "hs2_4.bin", 0x00001, 0x40000, CRC(132a2312) SHA1(9ad5c3a3ca895d290ff584f605f05d70386cfa10) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "hs_5.bin", 0x00000, 0x40000, CRC(09931910) SHA1(cac792f7c67d0ea274ecb369cef0554a033e8d88) )
	ROM_LOAD16_BYTE( "hs_6.bin", 0x00001, 0x40000, CRC(801703e4) SHA1(7da822b03a6d4f53a49bb1fedc9e1262d8a84782) )
ROM_END

ROM_START( hldspin2b )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "hs2b28r.bin", 0x00000, 0x40000, CRC(43c2a1b1) SHA1(da1e6d72e03297b014cb947e5c28769ad8457dec) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "hs_2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "hs2_3.bin", 0x00000, 0x40000, CRC(b6890061) SHA1(c196f8740d8487b108cff58d77a203b2d8431a67) )
	ROM_LOAD16_BYTE( "hs2_4.bin", 0x00001, 0x40000, CRC(132a2312) SHA1(9ad5c3a3ca895d290ff584f605f05d70386cfa10) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "hs_5.bin", 0x00000, 0x40000, CRC(09931910) SHA1(cac792f7c67d0ea274ecb369cef0554a033e8d88) )
	ROM_LOAD16_BYTE( "hs_6.bin", 0x00001, 0x40000, CRC(801703e4) SHA1(7da822b03a6d4f53a49bb1fedc9e1262d8a84782) )
ROM_END

ROM_START( hldspin2d )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "hs2d28r.bin", 0x00000, 0x40000, CRC(6e38ca1a) SHA1(9ef5522dfec75fa9b3809524f033e24817e325e3) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "hs_2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "hs2_3.bin", 0x00000, 0x40000, CRC(b6890061) SHA1(c196f8740d8487b108cff58d77a203b2d8431a67) )
	ROM_LOAD16_BYTE( "hs2_4.bin", 0x00001, 0x40000, CRC(132a2312) SHA1(9ad5c3a3ca895d290ff584f605f05d70386cfa10) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "hs_5.bin", 0x00000, 0x40000, CRC(09931910) SHA1(cac792f7c67d0ea274ecb369cef0554a033e8d88) )
	ROM_LOAD16_BYTE( "hs_6.bin", 0x00001, 0x40000, CRC(801703e4) SHA1(7da822b03a6d4f53a49bb1fedc9e1262d8a84782) )
ROM_END

ROM_START( hldspin2v )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "hs2v28r.bin", 0x00000, 0x40000, CRC(6f2fd1b3) SHA1(fe45508d95f61415dc1961a20ebb99f24b773c7d) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "hs_2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "hs2_3.bin", 0x00000, 0x40000, CRC(b6890061) SHA1(c196f8740d8487b108cff58d77a203b2d8431a67) )
	ROM_LOAD16_BYTE( "hs2_4.bin", 0x00001, 0x40000, CRC(132a2312) SHA1(9ad5c3a3ca895d290ff584f605f05d70386cfa10) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "hs_5.bin", 0x00000, 0x40000, CRC(09931910) SHA1(cac792f7c67d0ea274ecb369cef0554a033e8d88) )
	ROM_LOAD16_BYTE( "hs_6.bin", 0x00001, 0x40000, CRC(801703e4) SHA1(7da822b03a6d4f53a49bb1fedc9e1262d8a84782) )
ROM_END

ROM_START( pickwin )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pw25t.bin", 0x00000, 0x40000, CRC(9b6bd032) SHA1(241c772d191841c72e973d5dc494be445d6fd668) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "pw-2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "pw-3.bin", 0x00000, 0x40000, CRC(9a27acbd) SHA1(3a3a6d0b5f6eeeebcb2ccaff773b8971b3bfd3c7) )
	ROM_LOAD16_BYTE( "pw-4.bin", 0x00001, 0x40000, CRC(6b629619) SHA1(3842a7579f67866aa9744b0d6fa6a47c923be6fe) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "pw-5.bin", 0x00000, 0x40000, CRC(ec2ac284) SHA1(35cfab27db3740823b3cba821bd178d28e0be5f8) )
	ROM_LOAD16_BYTE( "pw-6.bin", 0x00001, 0x40000, CRC(aba36d00) SHA1(5abb1fe7d4f212fa0f7d5314f76e7c6b07e6c4bb) )
ROM_END

ROM_START( pickwina )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pw26.bin", 0x00000, 0x40000, CRC(9bedbe5a) SHA1(fb9ee63932b5f86fe42f84a5e1b8a3c29194761b) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "pw-2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "pw-3.bin", 0x00000, 0x40000, CRC(9a27acbd) SHA1(3a3a6d0b5f6eeeebcb2ccaff773b8971b3bfd3c7) )
	ROM_LOAD16_BYTE( "pw-4.bin", 0x00001, 0x40000, CRC(6b629619) SHA1(3842a7579f67866aa9744b0d6fa6a47c923be6fe) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "pw-5.bin", 0x00000, 0x40000, CRC(ec2ac284) SHA1(35cfab27db3740823b3cba821bd178d28e0be5f8) )
	ROM_LOAD16_BYTE( "pw-6.bin", 0x00001, 0x40000, CRC(aba36d00) SHA1(5abb1fe7d4f212fa0f7d5314f76e7c6b07e6c4bb) )
ROM_END

ROM_START( pickwinb )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pwb28t.bin", 0x00000, 0x40000, CRC(884ba143) SHA1(1210a9ee04468ef33902a358f4c1966f3a9169c9) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "pw-2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "pw-3.bin", 0x00000, 0x40000, CRC(9a27acbd) SHA1(3a3a6d0b5f6eeeebcb2ccaff773b8971b3bfd3c7) )
	ROM_LOAD16_BYTE( "pw-4.bin", 0x00001, 0x40000, CRC(6b629619) SHA1(3842a7579f67866aa9744b0d6fa6a47c923be6fe) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "pw-5.bin", 0x00000, 0x40000, CRC(ec2ac284) SHA1(35cfab27db3740823b3cba821bd178d28e0be5f8) )
	ROM_LOAD16_BYTE( "pw-6.bin", 0x00001, 0x40000, CRC(aba36d00) SHA1(5abb1fe7d4f212fa0f7d5314f76e7c6b07e6c4bb) )
ROM_END

ROM_START( pickwinb2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pwb29r.bin", 0x00000, 0x40000, CRC(cd28d461) SHA1(09c5994e3cd63995047c75339a4d93eb40043e97))

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "pw-2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "pw-3.bin", 0x00000, 0x40000, CRC(9a27acbd) SHA1(3a3a6d0b5f6eeeebcb2ccaff773b8971b3bfd3c7) )
	ROM_LOAD16_BYTE( "pw-4.bin", 0x00001, 0x40000, CRC(6b629619) SHA1(3842a7579f67866aa9744b0d6fa6a47c923be6fe) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "pw-5.bin", 0x00000, 0x40000, CRC(ec2ac284) SHA1(35cfab27db3740823b3cba821bd178d28e0be5f8) )
	ROM_LOAD16_BYTE( "pw-6.bin", 0x00001, 0x40000, CRC(aba36d00) SHA1(5abb1fe7d4f212fa0f7d5314f76e7c6b07e6c4bb) )
ROM_END

ROM_START( pickwind )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pwd28t.bin", 0x00000, 0x40000, CRC(8e3c50ee) SHA1(d673f89eb5755a0601c373874eb1789f9afd4ba3) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "pw-2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )



	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "pw-3.bin", 0x00000, 0x40000, CRC(9a27acbd) SHA1(3a3a6d0b5f6eeeebcb2ccaff773b8971b3bfd3c7) )
	ROM_LOAD16_BYTE( "pw-4.bin", 0x00001, 0x40000, CRC(6b629619) SHA1(3842a7579f67866aa9744b0d6fa6a47c923be6fe) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "pw-5.bin", 0x00000, 0x40000, CRC(ec2ac284) SHA1(35cfab27db3740823b3cba821bd178d28e0be5f8) )
	ROM_LOAD16_BYTE( "pw-6.bin", 0x00001, 0x40000, CRC(aba36d00) SHA1(5abb1fe7d4f212fa0f7d5314f76e7c6b07e6c4bb) )
ROM_END

ROM_START( pickwind2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pwd29r.bin", 0x00000, 0x40000, CRC(cb9f77e1) SHA1(5c851e70537ad4e418c3b6aca394bd2ecc4b4c08) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "pw-2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "pw-3.bin", 0x00000, 0x40000, CRC(9a27acbd) SHA1(3a3a6d0b5f6eeeebcb2ccaff773b8971b3bfd3c7) )
	ROM_LOAD16_BYTE( "pw-4.bin", 0x00001, 0x40000, CRC(6b629619) SHA1(3842a7579f67866aa9744b0d6fa6a47c923be6fe) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "pw-5.bin", 0x00000, 0x40000, CRC(ec2ac284) SHA1(35cfab27db3740823b3cba821bd178d28e0be5f8) )
	ROM_LOAD16_BYTE( "pw-6.bin", 0x00001, 0x40000, CRC(aba36d00) SHA1(5abb1fe7d4f212fa0f7d5314f76e7c6b07e6c4bb) )
ROM_END

ROM_START( pickwinv )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pwv28t.bin", 0x00000, 0x40000, CRC(2a523363) SHA1(cb6f0e4b3126ee6952c2eb5c789f8c1e368d12ee) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "pw-2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "pw-3.bin", 0x00000, 0x40000, CRC(9a27acbd) SHA1(3a3a6d0b5f6eeeebcb2ccaff773b8971b3bfd3c7) )
	ROM_LOAD16_BYTE( "pw-4.bin", 0x00001, 0x40000, CRC(6b629619) SHA1(3842a7579f67866aa9744b0d6fa6a47c923be6fe) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "pw-5.bin", 0x00000, 0x40000, CRC(ec2ac284) SHA1(35cfab27db3740823b3cba821bd178d28e0be5f8) )
	ROM_LOAD16_BYTE( "pw-6.bin", 0x00001, 0x40000, CRC(aba36d00) SHA1(5abb1fe7d4f212fa0f7d5314f76e7c6b07e6c4bb) )
ROM_END

ROM_START( pickwinv2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pwv29e.bin", 0x00000, 0x40000, CRC(9bd66421) SHA1(0bcaf151aecf31760e93199cf669a8b45293e98c))

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "pw-2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )


	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "pw-3.bin", 0x00000, 0x40000, CRC(9a27acbd) SHA1(3a3a6d0b5f6eeeebcb2ccaff773b8971b3bfd3c7) )
	ROM_LOAD16_BYTE( "pw-4.bin", 0x00001, 0x40000, CRC(6b629619) SHA1(3842a7579f67866aa9744b0d6fa6a47c923be6fe) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "pw-5.bin", 0x00000, 0x40000, CRC(ec2ac284) SHA1(35cfab27db3740823b3cba821bd178d28e0be5f8) )
	ROM_LOAD16_BYTE( "pw-6.bin", 0x00001, 0x40000, CRC(aba36d00) SHA1(5abb1fe7d4f212fa0f7d5314f76e7c6b07e6c4bb) )
ROM_END

ROM_START( pickwinv3 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pwv29r.bin", 0x00000, 0x40000, CRC(a08dcc45) SHA1(441256e9dd9fdc551a6e1c4e20b03a7a559d2a6c))

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "pw-2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "pw-3.bin", 0x00000, 0x40000, CRC(9a27acbd) SHA1(3a3a6d0b5f6eeeebcb2ccaff773b8971b3bfd3c7) )
	ROM_LOAD16_BYTE( "pw-4.bin", 0x00001, 0x40000, CRC(6b629619) SHA1(3842a7579f67866aa9744b0d6fa6a47c923be6fe) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "pw-5.bin", 0x00000, 0x40000, CRC(ec2ac284) SHA1(35cfab27db3740823b3cba821bd178d28e0be5f8) )
	ROM_LOAD16_BYTE( "pw-6.bin", 0x00001, 0x40000, CRC(aba36d00) SHA1(5abb1fe7d4f212fa0f7d5314f76e7c6b07e6c4bb) )
ROM_END

ROM_START( tighook )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "thk17xt.bin", 0x00000, 0x40000, CRC(02ca5fe2) SHA1(daa66d5ef7336e311cc8bb78ec6625620b9b2800) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "thkrom2.bin", 0x00000, 0x40000, CRC(61b61b75) SHA1(e71c5ab6aedb7ca4db32a2f4d7d2818dcdd92417) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "thkrom3.bin", 0x00000, 0x80000, CRC(ec4b4144) SHA1(36df0686b405a3c99707a6b63ad14bff1cd7b443) )
	ROM_LOAD16_BYTE( "thkrom4.bin", 0x00001, 0x80000, CRC(dbd1c526) SHA1(1f82f3e132bb5ac598e5d95254de48357130a0a0) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "thkrom5.bin", 0x00000, 0x80000, CRC(4085e345) SHA1(38c8e4727a782630527141e1586fcceff1d07d76) )
	ROM_LOAD16_BYTE( "thkrom6.bin", 0x00001, 0x80000, CRC(637695ff) SHA1(a5707b545968ac9d41c1a4ffd4de60a9df4bcbf1) )
ROM_END

ROM_START( tighooka )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "thk17.bin", 0x00000, 0x40000, CRC(0e27d3dd) SHA1(c85e2e03c36e0f6ec95e15597a6bd58e8eeb6353) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "thkrom2.bin", 0x00000, 0x40000, CRC(61b61b75) SHA1(e71c5ab6aedb7ca4db32a2f4d7d2818dcdd92417) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "thkrom3.bin", 0x00000, 0x80000, CRC(ec4b4144) SHA1(36df0686b405a3c99707a6b63ad14bff1cd7b443) )
	ROM_LOAD16_BYTE( "thkrom4.bin", 0x00001, 0x80000, CRC(dbd1c526) SHA1(1f82f3e132bb5ac598e5d95254de48357130a0a0) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "thkrom5.bin", 0x00000, 0x80000, CRC(4085e345) SHA1(38c8e4727a782630527141e1586fcceff1d07d76) )
	ROM_LOAD16_BYTE( "thkrom6.bin", 0x00001, 0x80000, CRC(637695ff) SHA1(a5707b545968ac9d41c1a4ffd4de60a9df4bcbf1) )
ROM_END



ROM_START( tighookc )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "thkc21r.bin", 0x00000, 0x40000, CRC(04bf78b1) SHA1(75408eb3fe67177ac5364cf72579ba09cf16b2fd) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "thkrom2.bin", 0x00000, 0x40000, CRC(61b61b75) SHA1(e71c5ab6aedb7ca4db32a2f4d7d2818dcdd92417) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "thkrom3.bin", 0x00000, 0x80000, CRC(ec4b4144) SHA1(36df0686b405a3c99707a6b63ad14bff1cd7b443) )
	ROM_LOAD16_BYTE( "thkrom4.bin", 0x00001, 0x80000, CRC(dbd1c526) SHA1(1f82f3e132bb5ac598e5d95254de48357130a0a0) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "thkrom5.bin", 0x00000, 0x80000, CRC(4085e345) SHA1(38c8e4727a782630527141e1586fcceff1d07d76) )
	ROM_LOAD16_BYTE( "thkrom6.bin", 0x00001, 0x80000, CRC(637695ff) SHA1(a5707b545968ac9d41c1a4ffd4de60a9df4bcbf1) )
ROM_END

ROM_START( tighookc2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "thkc20lt.bin", 0x00000, 0x40000, CRC(dc683f21) SHA1(f0e570b9570969dcff0c5349c5de9712c2abc754) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "thkrom2.bin", 0x00000, 0x40000, CRC(61b61b75) SHA1(e71c5ab6aedb7ca4db32a2f4d7d2818dcdd92417) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "thkrom3.bin", 0x00000, 0x80000, CRC(ec4b4144) SHA1(36df0686b405a3c99707a6b63ad14bff1cd7b443) )
	ROM_LOAD16_BYTE( "thkrom4.bin", 0x00001, 0x80000, CRC(dbd1c526) SHA1(1f82f3e132bb5ac598e5d95254de48357130a0a0) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "thkrom5.bin", 0x00000, 0x80000, CRC(4085e345) SHA1(38c8e4727a782630527141e1586fcceff1d07d76) )
	ROM_LOAD16_BYTE( "thkrom6.bin", 0x00001, 0x80000, CRC(637695ff) SHA1(a5707b545968ac9d41c1a4ffd4de60a9df4bcbf1) )
ROM_END

ROM_START( tighookv )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "thkv21r.bin", 0x00000, 0x40000, CRC(30ade52d) SHA1(ae59b7fd79581b3fa0b764648ccf34dc0fcc886e) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "thkrom2.bin", 0x00000, 0x40000, CRC(61b61b75) SHA1(e71c5ab6aedb7ca4db32a2f4d7d2818dcdd92417) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "thkrom3.bin", 0x00000, 0x80000, CRC(ec4b4144) SHA1(36df0686b405a3c99707a6b63ad14bff1cd7b443) )
	ROM_LOAD16_BYTE( "thkrom4.bin", 0x00001, 0x80000, CRC(dbd1c526) SHA1(1f82f3e132bb5ac598e5d95254de48357130a0a0) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "thkrom5.bin", 0x00000, 0x80000, CRC(4085e345) SHA1(38c8e4727a782630527141e1586fcceff1d07d76) )
	ROM_LOAD16_BYTE( "thkrom6.bin", 0x00001, 0x80000, CRC(637695ff) SHA1(a5707b545968ac9d41c1a4ffd4de60a9df4bcbf1) )
ROM_END

ROM_START( tighookv2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "thkv21e.bin", 0x00000, 0x40000, CRC(df0df2fa) SHA1(244086e9233f36531c005f6f9a09128738771753) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "thkrom2.bin", 0x00000, 0x40000, CRC(61b61b75) SHA1(e71c5ab6aedb7ca4db32a2f4d7d2818dcdd92417) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "thkrom3.bin", 0x00000, 0x80000, CRC(ec4b4144) SHA1(36df0686b405a3c99707a6b63ad14bff1cd7b443) )
	ROM_LOAD16_BYTE( "thkrom4.bin", 0x00001, 0x80000, CRC(dbd1c526) SHA1(1f82f3e132bb5ac598e5d95254de48357130a0a0) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "thkrom5.bin", 0x00000, 0x80000, CRC(4085e345) SHA1(38c8e4727a782630527141e1586fcceff1d07d76) )
	ROM_LOAD16_BYTE( "thkrom6.bin", 0x00001, 0x80000, CRC(637695ff) SHA1(a5707b545968ac9d41c1a4ffd4de60a9df4bcbf1) )
ROM_END

ROM_START( tighookv3 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "thkv20lt.bin", 0x00000, 0x40000, CRC(07a8e921) SHA1(2c92ec7187d441d1b205eea626d32a6a41a53918) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "thkrom2.bin", 0x00000, 0x40000, CRC(61b61b75) SHA1(e71c5ab6aedb7ca4db32a2f4d7d2818dcdd92417) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "thkrom3.bin", 0x00000, 0x80000, CRC(ec4b4144) SHA1(36df0686b405a3c99707a6b63ad14bff1cd7b443) )
	ROM_LOAD16_BYTE( "thkrom4.bin", 0x00001, 0x80000, CRC(dbd1c526) SHA1(1f82f3e132bb5ac598e5d95254de48357130a0a0) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "thkrom5.bin", 0x00000, 0x80000, CRC(4085e345) SHA1(38c8e4727a782630527141e1586fcceff1d07d76) )
	ROM_LOAD16_BYTE( "thkrom6.bin", 0x00001, 0x80000, CRC(637695ff) SHA1(a5707b545968ac9d41c1a4ffd4de60a9df4bcbf1) )
ROM_END

ROM_START( tighookd )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "thkd21r.bin", 0x00000, 0x40000,  CRC(407a2a93) SHA1(c729e5fc4b08ea0e0fcc2e6b4fd742b1dc461a0e) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "thkrom2.bin", 0x00000, 0x40000, CRC(61b61b75) SHA1(e71c5ab6aedb7ca4db32a2f4d7d2818dcdd92417) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "thkrom3.bin", 0x00000, 0x80000, CRC(ec4b4144) SHA1(36df0686b405a3c99707a6b63ad14bff1cd7b443) )
	ROM_LOAD16_BYTE( "thkrom4.bin", 0x00001, 0x80000, CRC(dbd1c526) SHA1(1f82f3e132bb5ac598e5d95254de48357130a0a0) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "thkrom5.bin", 0x00000, 0x80000, CRC(4085e345) SHA1(38c8e4727a782630527141e1586fcceff1d07d76) )
	ROM_LOAD16_BYTE( "thkrom6.bin", 0x00001, 0x80000, CRC(637695ff) SHA1(a5707b545968ac9d41c1a4ffd4de60a9df4bcbf1) )
ROM_END

ROM_START( tighookd2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "thkd20lt.bin", 0x00000, 0x40000, CRC(2be25e14) SHA1(2d906ce8d505bc2620ed218fdb401c0faf426eda) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "thkrom2.bin", 0x00000, 0x40000, CRC(61b61b75) SHA1(e71c5ab6aedb7ca4db32a2f4d7d2818dcdd92417) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "thkrom3.bin", 0x00000, 0x80000, CRC(ec4b4144) SHA1(36df0686b405a3c99707a6b63ad14bff1cd7b443) )
	ROM_LOAD16_BYTE( "thkrom4.bin", 0x00001, 0x80000, CRC(dbd1c526) SHA1(1f82f3e132bb5ac598e5d95254de48357130a0a0) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "thkrom5.bin", 0x00000, 0x80000, CRC(4085e345) SHA1(38c8e4727a782630527141e1586fcceff1d07d76) )
	ROM_LOAD16_BYTE( "thkrom6.bin", 0x00001, 0x80000, CRC(637695ff) SHA1(a5707b545968ac9d41c1a4ffd4de60a9df4bcbf1) )
ROM_END

ROM_START( robadv )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "ra15.bin", 0x00000, 0x40000, CRC(dd7e4ec9) SHA1(038b03855eaa8be1a97e34534822465a10886e10) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "rarom2.bin", 0x00000, 0x40000, CRC(092392cb) SHA1(fd52a0c4f46cb3242bf1b9e35ad5f41cda64010b) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "rarom3.bin", 0x00000, 0x80000, CRC(94e4cd71) SHA1(5c01e276dea3df7c367210af3d0d2399935c81c6) )
	ROM_LOAD16_BYTE( "rarom4.bin", 0x00001, 0x80000, CRC(72cfec99) SHA1(6612b8d04c0cc97dc5315fda861b606a6c158ea6) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "rarom5.bin", 0x00000, 0x80000, CRC(9bf41c2b) SHA1(8cc8ca5c2c63223e670e00ca5802b8677856bc16) )
	ROM_LOAD16_BYTE( "rarom6.bin", 0x00001, 0x80000, CRC(0fb69b4c) SHA1(8e1aaf5ade707b4045d55ff64e72cfe5db696332) )
ROM_END

ROM_START( robadvc )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "r1ac17r.bin", 0x00000, 0x40000, CRC(2e086ad9) SHA1(4cf96cf702fe38895d3ba3582cb7d74d79bc2208) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "rarom2.bin", 0x00000, 0x40000, CRC(092392cb) SHA1(fd52a0c4f46cb3242bf1b9e35ad5f41cda64010b) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "rarom3.bin", 0x00000, 0x80000, CRC(94e4cd71) SHA1(5c01e276dea3df7c367210af3d0d2399935c81c6) )
	ROM_LOAD16_BYTE( "rarom4.bin", 0x00001, 0x80000, CRC(72cfec99) SHA1(6612b8d04c0cc97dc5315fda861b606a6c158ea6) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "rarom5.bin", 0x00000, 0x80000, CRC(9bf41c2b) SHA1(8cc8ca5c2c63223e670e00ca5802b8677856bc16) )
	ROM_LOAD16_BYTE( "rarom6.bin", 0x00001, 0x80000, CRC(0fb69b4c) SHA1(8e1aaf5ade707b4045d55ff64e72cfe5db696332) )
ROM_END

ROM_START( robadvd )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "r1ad17r.bin", 0x00000, 0x40000, CRC(a00411d0) SHA1(007a3cf7bdd99a0200a2e34b89487f74a60c5561) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "rarom2.bin", 0x00000, 0x40000, CRC(092392cb) SHA1(fd52a0c4f46cb3242bf1b9e35ad5f41cda64010b) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "rarom3.bin", 0x00000, 0x80000, CRC(94e4cd71) SHA1(5c01e276dea3df7c367210af3d0d2399935c81c6) )
	ROM_LOAD16_BYTE( "rarom4.bin", 0x00001, 0x80000, CRC(72cfec99) SHA1(6612b8d04c0cc97dc5315fda861b606a6c158ea6) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "rarom5.bin", 0x00000, 0x80000, CRC(9bf41c2b) SHA1(8cc8ca5c2c63223e670e00ca5802b8677856bc16) )
	ROM_LOAD16_BYTE( "rarom6.bin", 0x00001, 0x80000, CRC(0fb69b4c) SHA1(8e1aaf5ade707b4045d55ff64e72cfe5db696332) )
ROM_END

ROM_START( robadvv1 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "r1av17e.bin", 0x00000, 0x40000, CRC(75c6960a) SHA1(9ca85f04bf5549027dd89f47ddb78f2618d4620c) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "rarom2.bin", 0x00000, 0x40000, CRC(092392cb) SHA1(fd52a0c4f46cb3242bf1b9e35ad5f41cda64010b) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "rarom3.bin", 0x00000, 0x80000, CRC(94e4cd71) SHA1(5c01e276dea3df7c367210af3d0d2399935c81c6) )
	ROM_LOAD16_BYTE( "rarom4.bin", 0x00001, 0x80000, CRC(72cfec99) SHA1(6612b8d04c0cc97dc5315fda861b606a6c158ea6) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "rarom5.bin", 0x00000, 0x80000, CRC(9bf41c2b) SHA1(8cc8ca5c2c63223e670e00ca5802b8677856bc16) )
	ROM_LOAD16_BYTE( "rarom6.bin", 0x00001, 0x80000, CRC(0fb69b4c) SHA1(8e1aaf5ade707b4045d55ff64e72cfe5db696332) )
ROM_END

ROM_START( robadvv2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "r1av17r.bin", 0x00000, 0x40000, CRC(1f97fa41) SHA1(b148bac2d96549a15135fe2a8a72913b880aa6c2) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "rarom2.bin", 0x00000, 0x40000, CRC(092392cb) SHA1(fd52a0c4f46cb3242bf1b9e35ad5f41cda64010b) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "rarom3.bin", 0x00000, 0x80000, CRC(94e4cd71) SHA1(5c01e276dea3df7c367210af3d0d2399935c81c6) )
	ROM_LOAD16_BYTE( "rarom4.bin", 0x00001, 0x80000, CRC(72cfec99) SHA1(6612b8d04c0cc97dc5315fda861b606a6c158ea6) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "rarom5.bin", 0x00000, 0x80000, CRC(9bf41c2b) SHA1(8cc8ca5c2c63223e670e00ca5802b8677856bc16) )
	ROM_LOAD16_BYTE( "rarom6.bin", 0x00001, 0x80000, CRC(0fb69b4c) SHA1(8e1aaf5ade707b4045d55ff64e72cfe5db696332) )
ROM_END




ROM_START( anibonus )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "ab15xt.bin", 0x00000, 0x40000,  CRC(3aed6e7f) SHA1(51f9af92286e8b2fcfeae30913fbab4626decb99) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "abrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "abrom3n.bin", 0x00000, 0x80000, CRC(aab2161a) SHA1(d472746c68720935fedfc6b2d06a4fe1152cc804) )
	ROM_LOAD16_BYTE( "abrom4n.bin", 0x00001, 0x80000, CRC(d776862c) SHA1(03b3c0e9adb11b560b8773e88ea97e712323f25e) )


	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "abrom5n.bin", 0x00000, 0x80000, CRC(d3db86eb) SHA1(e7e2cdfa6b4795d4021f589d2a292c67cc32f03a) )
	ROM_LOAD16_BYTE( "abrom6n.bin", 0x00001, 0x80000, CRC(0d8dcaa1) SHA1(a74c64bb89b4273e9d1e092786a5cf8ebd60477c) )
ROM_END

ROM_START( anibonus2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "ab15.bin", 0x00000, 0x40000, CRC(4640a2e7) SHA1(2659c037e88f43f89a5d8cd563eec5e4eb2025b9) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "abrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "abrom3n.bin", 0x00000, 0x80000, CRC(aab2161a) SHA1(d472746c68720935fedfc6b2d06a4fe1152cc804) )
	ROM_LOAD16_BYTE( "abrom4n.bin", 0x00001, 0x80000, CRC(d776862c) SHA1(03b3c0e9adb11b560b8773e88ea97e712323f25e) )


	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "abrom5n.bin", 0x00000, 0x80000, CRC(d3db86eb) SHA1(e7e2cdfa6b4795d4021f589d2a292c67cc32f03a) )
	ROM_LOAD16_BYTE( "abrom6n.bin", 0x00001, 0x80000, CRC(0d8dcaa1) SHA1(a74c64bb89b4273e9d1e092786a5cf8ebd60477c) )
ROM_END

ROM_START( anibonus3 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "ab14xta.bin", 0x00000, 0x40000,  CRC(eddf38af) SHA1(56a920ba1af213719210d25e6d8b5c7a0d513119) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "abrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )


	/* unsure which gfx roms */
	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "abrom3.bin", 0x00000, 0x80000, CRC(85f19e19) SHA1(2dd259af132e0cfd34974526c96f0d96ff868516) )
	ROM_LOAD16_BYTE( "abrom4.bin", 0x00001, 0x80000, CRC(c12b954c) SHA1(83556abbad0f285360da5f5e0fb93514d46b436b) )

	ROM_LOAD16_BYTE( "abrom3a.bin", 0x00000, 0x80000, CRC(85f19e19) SHA1(2dd259af132e0cfd34974526c96f0d96ff868516) )
	ROM_LOAD16_BYTE( "abrom4a.bin", 0x00001, 0x80000, CRC(c12b954c) SHA1(83556abbad0f285360da5f5e0fb93514d46b436b) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "abrom5.bin", 0x00000, 0x80000, CRC(74b4fa88) SHA1(922d9c4f864be2b125269a69639e6206aec26d72) )
	ROM_LOAD16_BYTE( "abrom6.bin", 0x00001, 0x80000, CRC(e8f4b079) SHA1(2597fa17b6a13e634ba9fe846661d09c65fa8cf2) )

	ROM_LOAD16_BYTE( "abrom5a.bin", 0x00000, 0x80000, CRC(9810f1e2) SHA1(a10954a46d52c5a53a3b11a04e66c4ed3ce2a0f7) )
	ROM_LOAD16_BYTE( "abrom6a.bin", 0x00001, 0x80000, CRC(22d2abbe) SHA1(65d82ed0fc799c4248696f1b2ef76e7e88bf7fb7) )
ROM_END


ROM_START( anibonus4 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "ab14xt.bin", 0x00000, 0x40000,  CRC(c6107445) SHA1(22fd3a7987219a940b965c953494939e0892661e) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "abrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	/* unsure which gfx roms */
	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "abrom3.bin", 0x00000, 0x80000, CRC(85f19e19) SHA1(2dd259af132e0cfd34974526c96f0d96ff868516) )
	ROM_LOAD16_BYTE( "abrom4.bin", 0x00001, 0x80000, CRC(c12b954c) SHA1(83556abbad0f285360da5f5e0fb93514d46b436b) )

	ROM_LOAD16_BYTE( "abrom3a.bin", 0x00000, 0x80000, CRC(85f19e19) SHA1(2dd259af132e0cfd34974526c96f0d96ff868516) )
	ROM_LOAD16_BYTE( "abrom4a.bin", 0x00001, 0x80000, CRC(c12b954c) SHA1(83556abbad0f285360da5f5e0fb93514d46b436b) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "abrom5.bin", 0x00000, 0x80000, CRC(74b4fa88) SHA1(922d9c4f864be2b125269a69639e6206aec26d72) )
	ROM_LOAD16_BYTE( "abrom6.bin", 0x00001, 0x80000, CRC(e8f4b079) SHA1(2597fa17b6a13e634ba9fe846661d09c65fa8cf2) )

	ROM_LOAD16_BYTE( "abrom5a.bin", 0x00000, 0x80000, CRC(9810f1e2) SHA1(a10954a46d52c5a53a3b11a04e66c4ed3ce2a0f7) )
	ROM_LOAD16_BYTE( "abrom6a.bin", 0x00001, 0x80000, CRC(22d2abbe) SHA1(65d82ed0fc799c4248696f1b2ef76e7e88bf7fb7) )
ROM_END

ROM_START( anibonus5 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "ab14a.bin", 0x00000, 0x40000, CRC(a8a0eea5) SHA1(c37a470b997ee5dbc976858c024bd67ed88061ce) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "abrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	/* unsure which gfx roms */
	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "abrom3.bin", 0x00000, 0x80000, CRC(85f19e19) SHA1(2dd259af132e0cfd34974526c96f0d96ff868516) )
	ROM_LOAD16_BYTE( "abrom4.bin", 0x00001, 0x80000, CRC(c12b954c) SHA1(83556abbad0f285360da5f5e0fb93514d46b436b) )

	ROM_LOAD16_BYTE( "abrom3a.bin", 0x00000, 0x80000, CRC(85f19e19) SHA1(2dd259af132e0cfd34974526c96f0d96ff868516) )
	ROM_LOAD16_BYTE( "abrom4a.bin", 0x00001, 0x80000, CRC(c12b954c) SHA1(83556abbad0f285360da5f5e0fb93514d46b436b) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "abrom5.bin", 0x00000, 0x80000, CRC(74b4fa88) SHA1(922d9c4f864be2b125269a69639e6206aec26d72) )
	ROM_LOAD16_BYTE( "abrom6.bin", 0x00001, 0x80000, CRC(e8f4b079) SHA1(2597fa17b6a13e634ba9fe846661d09c65fa8cf2) )

	ROM_LOAD16_BYTE( "abrom5a.bin", 0x00000, 0x80000, CRC(9810f1e2) SHA1(a10954a46d52c5a53a3b11a04e66c4ed3ce2a0f7) )
	ROM_LOAD16_BYTE( "abrom6a.bin", 0x00001, 0x80000, CRC(22d2abbe) SHA1(65d82ed0fc799c4248696f1b2ef76e7e88bf7fb7) )
ROM_END

ROM_START( anibonus6 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "ab14.bin", 0x00000, 0x40000, CRC(d1dcb6e6) SHA1(4a95184e5d4f2e0527fdc8f29e56572cf3ba9987) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "abrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	/* unsure which gfx roms */
	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "abrom3.bin", 0x00000, 0x80000, CRC(85f19e19) SHA1(2dd259af132e0cfd34974526c96f0d96ff868516) )
	ROM_LOAD16_BYTE( "abrom4.bin", 0x00001, 0x80000, CRC(c12b954c) SHA1(83556abbad0f285360da5f5e0fb93514d46b436b) )

	ROM_LOAD16_BYTE( "abrom3a.bin", 0x00000, 0x80000, CRC(85f19e19) SHA1(2dd259af132e0cfd34974526c96f0d96ff868516) )
	ROM_LOAD16_BYTE( "abrom4a.bin", 0x00001, 0x80000, CRC(c12b954c) SHA1(83556abbad0f285360da5f5e0fb93514d46b436b) )



	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "abrom5.bin", 0x00000, 0x80000, CRC(74b4fa88) SHA1(922d9c4f864be2b125269a69639e6206aec26d72) )
	ROM_LOAD16_BYTE( "abrom6.bin", 0x00001, 0x80000, CRC(e8f4b079) SHA1(2597fa17b6a13e634ba9fe846661d09c65fa8cf2) )

	ROM_LOAD16_BYTE( "abrom5a.bin", 0x00000, 0x80000, CRC(9810f1e2) SHA1(a10954a46d52c5a53a3b11a04e66c4ed3ce2a0f7) )
	ROM_LOAD16_BYTE( "abrom6a.bin", 0x00001, 0x80000, CRC(22d2abbe) SHA1(65d82ed0fc799c4248696f1b2ef76e7e88bf7fb7) )
ROM_END


ROM_START( anibonusb )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "abb17r.bin", 0x00000, 0x40000, CRC(e49e6dfc) SHA1(358448f7f68ba53e8c9c04a8a0e54f1ba292705f) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "abrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "abrom3n.bin", 0x00000, 0x80000, CRC(aab2161a) SHA1(d472746c68720935fedfc6b2d06a4fe1152cc804) )
	ROM_LOAD16_BYTE( "abrom4n.bin", 0x00001, 0x80000, CRC(d776862c) SHA1(03b3c0e9adb11b560b8773e88ea97e712323f25e) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "abrom5n.bin", 0x00000, 0x80000, CRC(d3db86eb) SHA1(e7e2cdfa6b4795d4021f589d2a292c67cc32f03a) )
	ROM_LOAD16_BYTE( "abrom6n.bin", 0x00001, 0x80000, CRC(0d8dcaa1) SHA1(a74c64bb89b4273e9d1e092786a5cf8ebd60477c) )
ROM_END

ROM_START( anibonusb2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "abb17lt.bin", 0x00000, 0x40000, CRC(fd600bf2) SHA1(13b3685e1cced585af08d711f24688a9f4e1ff8c) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "abrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "abrom3n.bin", 0x00000, 0x80000, CRC(aab2161a) SHA1(d472746c68720935fedfc6b2d06a4fe1152cc804) )
	ROM_LOAD16_BYTE( "abrom4n.bin", 0x00001, 0x80000, CRC(d776862c) SHA1(03b3c0e9adb11b560b8773e88ea97e712323f25e) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "abrom5n.bin", 0x00000, 0x80000, CRC(d3db86eb) SHA1(e7e2cdfa6b4795d4021f589d2a292c67cc32f03a) )
	ROM_LOAD16_BYTE( "abrom6n.bin", 0x00001, 0x80000, CRC(0d8dcaa1) SHA1(a74c64bb89b4273e9d1e092786a5cf8ebd60477c) )
ROM_END

ROM_START( anibonusd )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "abd17r.bin", 0x00000, 0x40000, CRC(32707445) SHA1(12005139862b209e0f187e27f61f779de81066a1) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "abrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "abrom3n.bin", 0x00000, 0x80000, CRC(aab2161a) SHA1(d472746c68720935fedfc6b2d06a4fe1152cc804) )
	ROM_LOAD16_BYTE( "abrom4n.bin", 0x00001, 0x80000, CRC(d776862c) SHA1(03b3c0e9adb11b560b8773e88ea97e712323f25e) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "abrom5n.bin", 0x00000, 0x80000, CRC(d3db86eb) SHA1(e7e2cdfa6b4795d4021f589d2a292c67cc32f03a) )
	ROM_LOAD16_BYTE( "abrom6n.bin", 0x00001, 0x80000, CRC(0d8dcaa1) SHA1(a74c64bb89b4273e9d1e092786a5cf8ebd60477c) )
ROM_END

ROM_START( anibonusd2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "abd17lt.bin", 0x00000, 0x40000, CRC(c718f9ab) SHA1(fdd9de6bd0a8e477412d8a9f1a442fec3361a067) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "abrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "abrom3n.bin", 0x00000, 0x80000, CRC(aab2161a) SHA1(d472746c68720935fedfc6b2d06a4fe1152cc804) )
	ROM_LOAD16_BYTE( "abrom4n.bin", 0x00001, 0x80000, CRC(d776862c) SHA1(03b3c0e9adb11b560b8773e88ea97e712323f25e) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "abrom5n.bin", 0x00000, 0x80000, CRC(d3db86eb) SHA1(e7e2cdfa6b4795d4021f589d2a292c67cc32f03a) )
	ROM_LOAD16_BYTE( "abrom6n.bin", 0x00001, 0x80000, CRC(0d8dcaa1) SHA1(a74c64bb89b4273e9d1e092786a5cf8ebd60477c) )
ROM_END

ROM_START( anibonusv )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "abv18r.bin", 0x00000, 0x40000, CRC(56672865) SHA1(44d141b307a2cb0cb4731ad6db8235941f80ae23) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "abrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "abrom3n.bin", 0x00000, 0x80000, CRC(aab2161a) SHA1(d472746c68720935fedfc6b2d06a4fe1152cc804) )
	ROM_LOAD16_BYTE( "abrom4n.bin", 0x00001, 0x80000, CRC(d776862c) SHA1(03b3c0e9adb11b560b8773e88ea97e712323f25e) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "abrom5n.bin", 0x00000, 0x80000, CRC(d3db86eb) SHA1(e7e2cdfa6b4795d4021f589d2a292c67cc32f03a) )
	ROM_LOAD16_BYTE( "abrom6n.bin", 0x00001, 0x80000, CRC(0d8dcaa1) SHA1(a74c64bb89b4273e9d1e092786a5cf8ebd60477c) )
ROM_END

ROM_START( anibonusv2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "abv18lt.bin", 0x00000, 0x40000, CRC(26bc1901) SHA1(c17f6bf5380c3c141cc79f4fb2e01bb8299e93b0) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "abrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "abrom3n.bin", 0x00000, 0x80000, CRC(aab2161a) SHA1(d472746c68720935fedfc6b2d06a4fe1152cc804) )
	ROM_LOAD16_BYTE( "abrom4n.bin", 0x00001, 0x80000, CRC(d776862c) SHA1(03b3c0e9adb11b560b8773e88ea97e712323f25e) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "abrom5n.bin", 0x00000, 0x80000, CRC(d3db86eb) SHA1(e7e2cdfa6b4795d4021f589d2a292c67cc32f03a) )
	ROM_LOAD16_BYTE( "abrom6n.bin", 0x00001, 0x80000, CRC(0d8dcaa1) SHA1(a74c64bb89b4273e9d1e092786a5cf8ebd60477c) )
ROM_END

ROM_START( anibonusv3 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "abv18e.bin", 0x00000, 0x40000, CRC(c05b8fb5) SHA1(8ae4e00a66d2825ceea072c58750915618477304) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "abrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "abrom3n.bin", 0x00000, 0x80000, CRC(aab2161a) SHA1(d472746c68720935fedfc6b2d06a4fe1152cc804) )
	ROM_LOAD16_BYTE( "abrom4n.bin", 0x00001, 0x80000, CRC(d776862c) SHA1(03b3c0e9adb11b560b8773e88ea97e712323f25e) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "abrom5n.bin", 0x00000, 0x80000, CRC(d3db86eb) SHA1(e7e2cdfa6b4795d4021f589d2a292c67cc32f03a) )
	ROM_LOAD16_BYTE( "abrom6n.bin", 0x00001, 0x80000, CRC(0d8dcaa1) SHA1(a74c64bb89b4273e9d1e092786a5cf8ebd60477c) )
ROM_END

ROM_START( abnudge )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "ab17n.bin", 0x00000, 0x40000, CRC(aca7c2af) SHA1(8f23b4aff006fcd983769f833c2fabdbb087d36b) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "abrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "abrom3n.bin", 0x00000, 0x80000, CRC(aab2161a) SHA1(d472746c68720935fedfc6b2d06a4fe1152cc804) )
	ROM_LOAD16_BYTE( "abrom4n.bin", 0x00001, 0x80000, CRC(d776862c) SHA1(03b3c0e9adb11b560b8773e88ea97e712323f25e) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "abrom5n.bin", 0x00000, 0x80000, CRC(d3db86eb) SHA1(e7e2cdfa6b4795d4021f589d2a292c67cc32f03a) )
	ROM_LOAD16_BYTE( "abrom6n.bin", 0x00001, 0x80000, CRC(0d8dcaa1) SHA1(a74c64bb89b4273e9d1e092786a5cf8ebd60477c) )
ROM_END

ROM_START( abnudgeb )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "abb20n.bin", 0x00000, 0x40000,  CRC(b202b40f) SHA1(fff2662b8c98aa1496b87df65177996b15b5befe) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "abrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "abrom3n.bin", 0x00000, 0x80000, CRC(aab2161a) SHA1(d472746c68720935fedfc6b2d06a4fe1152cc804) )
	ROM_LOAD16_BYTE( "abrom4n.bin", 0x00001, 0x80000, CRC(d776862c) SHA1(03b3c0e9adb11b560b8773e88ea97e712323f25e) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "abrom5n.bin", 0x00000, 0x80000, CRC(d3db86eb) SHA1(e7e2cdfa6b4795d4021f589d2a292c67cc32f03a) )
	ROM_LOAD16_BYTE( "abrom6n.bin", 0x00001, 0x80000, CRC(0d8dcaa1) SHA1(a74c64bb89b4273e9d1e092786a5cf8ebd60477c) )
ROM_END

ROM_START( abnudged )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "abd20n.bin", 0x00000, 0x40000, CRC(e189ca0b) SHA1(ba3a3f84b302b737043ac56b0872d65c4ea77903) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "abrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "abrom3n.bin", 0x00000, 0x80000, CRC(aab2161a) SHA1(d472746c68720935fedfc6b2d06a4fe1152cc804) )
	ROM_LOAD16_BYTE( "abrom4n.bin", 0x00001, 0x80000, CRC(d776862c) SHA1(03b3c0e9adb11b560b8773e88ea97e712323f25e) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "abrom5n.bin", 0x00000, 0x80000, CRC(d3db86eb) SHA1(e7e2cdfa6b4795d4021f589d2a292c67cc32f03a) )
	ROM_LOAD16_BYTE( "abrom6n.bin", 0x00001, 0x80000, CRC(0d8dcaa1) SHA1(a74c64bb89b4273e9d1e092786a5cf8ebd60477c) )
ROM_END

ROM_START( abnudgev )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "abv21n.bin", 0x00000, 0x40000, CRC(48d8f3a6) SHA1(5ccde4bf574ba779dc43769fda62aa6d9b284a8e) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "abrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "abrom3n.bin", 0x00000, 0x80000, CRC(aab2161a) SHA1(d472746c68720935fedfc6b2d06a4fe1152cc804) )
	ROM_LOAD16_BYTE( "abrom4n.bin", 0x00001, 0x80000, CRC(d776862c) SHA1(03b3c0e9adb11b560b8773e88ea97e712323f25e) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "abrom5n.bin", 0x00000, 0x80000, CRC(d3db86eb) SHA1(e7e2cdfa6b4795d4021f589d2a292c67cc32f03a) )
	ROM_LOAD16_BYTE( "abrom6n.bin", 0x00001, 0x80000, CRC(0d8dcaa1) SHA1(a74c64bb89b4273e9d1e092786a5cf8ebd60477c) )
ROM_END

ROM_START( anithunt )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "ath17.bin", 0x00000, 0x40000, CRC(07facf55) SHA1(2de5ca12e06a6896099672ec7383e6324d23fa12) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "athrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "athrom3.bin", 0x00000, 0x80000, CRC(f784ec01) SHA1(69474fc9d10882fd9ec0c02675193df7aa31f6a7) )
	ROM_LOAD16_BYTE( "athrom4.bin", 0x00001, 0x80000, CRC(49749939) SHA1(6deb10c2b51b5718f0cba31f6bda54bcc001bc71) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "ath-rom5.bin", 0x00000, 0x80000, CRC(536a7e23) SHA1(51dc6b2b022a672810b00e1006b0c7ee610a4e4f) )
	ROM_LOAD16_BYTE( "ath-rom6.bin", 0x00001, 0x80000, CRC(23bc5067) SHA1(1e279e58437b897c7a68c9cdd15277c6a906a142) )
ROM_END

ROM_START( anithunt2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "ath15.bin", 0x00000, 0x40000, CRC(917ae674) SHA1(67808a9d3bd48a8f7f839eb85356269a357581ad) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "athrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "ath-rom3.bin", 0x00000, 0x80000, CRC(2ce266b2) SHA1(34dcc504d48a26976e17ad0b8399904e5ecc3379) )
	ROM_LOAD16_BYTE( "ath-rom4.bin", 0x00001, 0x80000, CRC(59d25672) SHA1(212ba0aa7794b7a37121896190e64069f005b1ea) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "ath-rom5.bin", 0x00000, 0x80000, CRC(536a7e23) SHA1(51dc6b2b022a672810b00e1006b0c7ee610a4e4f) )
	ROM_LOAD16_BYTE( "ath-rom6.bin", 0x00001, 0x80000, CRC(23bc5067) SHA1(1e279e58437b897c7a68c9cdd15277c6a906a142) )
ROM_END

ROM_START( anithuntb )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "athb19r.bin", 0x00000, 0x40000, CRC(71d0604f) SHA1(c2f40c58dce2f6b69dc0234c0fb7a656ea04168b) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "athrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "athrom3.bin", 0x00000, 0x80000, CRC(f784ec01) SHA1(69474fc9d10882fd9ec0c02675193df7aa31f6a7) )
	ROM_LOAD16_BYTE( "athrom4.bin", 0x00001, 0x80000, CRC(49749939) SHA1(6deb10c2b51b5718f0cba31f6bda54bcc001bc71) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "ath-rom5.bin", 0x00000, 0x80000, CRC(536a7e23) SHA1(51dc6b2b022a672810b00e1006b0c7ee610a4e4f) )
	ROM_LOAD16_BYTE( "ath-rom6.bin", 0x00001, 0x80000, CRC(23bc5067) SHA1(1e279e58437b897c7a68c9cdd15277c6a906a142) )
ROM_END

ROM_START( anithuntd )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "athd19r.bin", 0x00000, 0x40000, CRC(807585d4) SHA1(643ceb51e81797b330310ddbe9e0d8b21ba215e5) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "athrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "athrom3.bin", 0x00000, 0x80000, CRC(f784ec01) SHA1(69474fc9d10882fd9ec0c02675193df7aa31f6a7) )
	ROM_LOAD16_BYTE( "athrom4.bin", 0x00001, 0x80000, CRC(49749939) SHA1(6deb10c2b51b5718f0cba31f6bda54bcc001bc71) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "ath-rom5.bin", 0x00000, 0x80000, CRC(536a7e23) SHA1(51dc6b2b022a672810b00e1006b0c7ee610a4e4f) )
	ROM_LOAD16_BYTE( "ath-rom6.bin", 0x00001, 0x80000, CRC(23bc5067) SHA1(1e279e58437b897c7a68c9cdd15277c6a906a142) )
ROM_END

ROM_START( anithuntv )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "athv19r.bin", 0x00000, 0x40000, CRC(74c2cf89) SHA1(f3efad66f668a0a6dbf35a0c6518ece842d069e6) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "athrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "athrom3.bin", 0x00000, 0x80000, CRC(f784ec01) SHA1(69474fc9d10882fd9ec0c02675193df7aa31f6a7) )
	ROM_LOAD16_BYTE( "athrom4.bin", 0x00001, 0x80000, CRC(49749939) SHA1(6deb10c2b51b5718f0cba31f6bda54bcc001bc71) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "ath-rom5.bin", 0x00000, 0x80000, CRC(536a7e23) SHA1(51dc6b2b022a672810b00e1006b0c7ee610a4e4f) )
	ROM_LOAD16_BYTE( "ath-rom6.bin", 0x00001, 0x80000, CRC(23bc5067) SHA1(1e279e58437b897c7a68c9cdd15277c6a906a142) )
ROM_END


ROM_START( dblchal )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "dc11.bin", 0x00000, 0x40000, CRC(05a27f07) SHA1(02b7b2731f8821bd7e0e3be005bd3024db0a7e42) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "dcrom2.bin", 0x00000, 0x20000, CRC(ce099327) SHA1(b1dc43839e9e3bf788141d58c81e1380a12d582a) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "dcrom3.bin", 0x00000, 0x40000, CRC(50b1c522) SHA1(620f8a1df6954c5db4a85448c810901d69859fec) )
	ROM_LOAD16_BYTE( "dcrom4.bin", 0x00001, 0x40000, CRC(a6f46957) SHA1(b4c0a28e428b9fc091ac5fc041a1ce01f65ff402) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "dcrom5.bin", 0x00000, 0x80000, CRC(a55f4ed3) SHA1(9f7427357af84026c056624523fd20bc556f3c22) )
	ROM_LOAD16_BYTE( "dcrom6.bin", 0x00001, 0x80000, CRC(cf783d82) SHA1(d3f8ae5cb3a5f848e2d84721a5a4ee486a52de85) )
ROM_END

ROM_START( dblchalb )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "dcb15r.bin", 0x00000, 0x40000, CRC(d89a9756) SHA1(7a4cb88da9d02351a996202fb5b4545db042867b) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "dcrom2.bin", 0x00000, 0x20000, CRC(ce099327) SHA1(b1dc43839e9e3bf788141d58c81e1380a12d582a) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "dcrom3.bin", 0x00000, 0x40000, CRC(50b1c522) SHA1(620f8a1df6954c5db4a85448c810901d69859fec) )
	ROM_LOAD16_BYTE( "dcrom4.bin", 0x00001, 0x40000, CRC(a6f46957) SHA1(b4c0a28e428b9fc091ac5fc041a1ce01f65ff402) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "dcrom5.bin", 0x00000, 0x80000, CRC(a55f4ed3) SHA1(9f7427357af84026c056624523fd20bc556f3c22) )
	ROM_LOAD16_BYTE( "dcrom6.bin", 0x00001, 0x80000, CRC(cf783d82) SHA1(d3f8ae5cb3a5f848e2d84721a5a4ee486a52de85) )
ROM_END

ROM_START( dblchalc )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "dcc15r.bin", 0x00000, 0x40000, CRC(ac0ed555) SHA1(5ac93132a94fec8811b4b5525dd2d31eb6749d6e) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "dcrom2.bin", 0x00000, 0x20000, CRC(ce099327) SHA1(b1dc43839e9e3bf788141d58c81e1380a12d582a) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "dcrom3.bin", 0x00000, 0x40000, CRC(50b1c522) SHA1(620f8a1df6954c5db4a85448c810901d69859fec) )
	ROM_LOAD16_BYTE( "dcrom4.bin", 0x00001, 0x40000, CRC(a6f46957) SHA1(b4c0a28e428b9fc091ac5fc041a1ce01f65ff402) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "dcrom5.bin", 0x00000, 0x80000, CRC(a55f4ed3) SHA1(9f7427357af84026c056624523fd20bc556f3c22) )
	ROM_LOAD16_BYTE( "dcrom6.bin", 0x00001, 0x80000, CRC(cf783d82) SHA1(d3f8ae5cb3a5f848e2d84721a5a4ee486a52de85) )
ROM_END

ROM_START( dblchald )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "dcd15r.bin", 0x00000, 0x40000, CRC(2b72350d) SHA1(439765028417af6ceeb2724c7b7e737a209bf844) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "dcrom2.bin", 0x00000, 0x20000, CRC(ce099327) SHA1(b1dc43839e9e3bf788141d58c81e1380a12d582a) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "dcrom3.bin", 0x00000, 0x40000, CRC(50b1c522) SHA1(620f8a1df6954c5db4a85448c810901d69859fec) )
	ROM_LOAD16_BYTE( "dcrom4.bin", 0x00001, 0x40000, CRC(a6f46957) SHA1(b4c0a28e428b9fc091ac5fc041a1ce01f65ff402) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "dcrom5.bin", 0x00000, 0x80000, CRC(a55f4ed3) SHA1(9f7427357af84026c056624523fd20bc556f3c22) )
	ROM_LOAD16_BYTE( "dcrom6.bin", 0x00001, 0x80000, CRC(cf783d82) SHA1(d3f8ae5cb3a5f848e2d84721a5a4ee486a52de85) )
ROM_END

ROM_START( dblchalv )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "dcv15r.bin", 0x00000, 0x40000, CRC(1e5fc8fd) SHA1(9b688966bd52828fde31003510ee6a2a3444525d) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "dcrom2.bin", 0x00000, 0x20000, CRC(ce099327) SHA1(b1dc43839e9e3bf788141d58c81e1380a12d582a) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "dcrom3.bin", 0x00000, 0x40000, CRC(50b1c522) SHA1(620f8a1df6954c5db4a85448c810901d69859fec) )
	ROM_LOAD16_BYTE( "dcrom4.bin", 0x00001, 0x40000, CRC(a6f46957) SHA1(b4c0a28e428b9fc091ac5fc041a1ce01f65ff402) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "dcrom5.bin", 0x00000, 0x80000, CRC(a55f4ed3) SHA1(9f7427357af84026c056624523fd20bc556f3c22) )
	ROM_LOAD16_BYTE( "dcrom6.bin", 0x00001, 0x80000, CRC(cf783d82) SHA1(d3f8ae5cb3a5f848e2d84721a5a4ee486a52de85) )
ROM_END


ROM_START( robadv2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "r2a15sh.bin", 0x00000, 0x40000, CRC(c53af9be) SHA1(86cb2dae1315227f01f430d23fb4e09d015f1206) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "rarom2.bin", 0x00000, 0x40000, CRC(092392cb) SHA1(fd52a0c4f46cb3242bf1b9e35ad5f41cda64010b) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "ra2rom3.bin", 0x00000, 0x80000, CRC(eacd0cf7) SHA1(a04ddc339d330be4b278f12a54fe65d7eb08ffd0) )
	ROM_LOAD16_BYTE( "ra2rom4.bin", 0x00001, 0x80000, CRC(adac68d2) SHA1(6287a979a57004b1c4eea7f539550e23bac22904) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "ra2rom5.bin", 0x00000, 0x80000, CRC(ad9379a2) SHA1(93126969677cfce20e5b2e287662ac6b2ceee425) )
	ROM_LOAD16_BYTE( "ra2rom6.bin", 0x00001, 0x80000, CRC(12312874) SHA1(9d4d9d9fbec8536e8a003892643654d15f4535fa) )
ROM_END

ROM_START( robadv2a )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "r2a15.bin", 0x00000, 0x40000, CRC(e1932e13) SHA1(918d51e64aefaa308f92748bb5bfa92b88e00feb) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "rarom2.bin", 0x00000, 0x40000, CRC(092392cb) SHA1(fd52a0c4f46cb3242bf1b9e35ad5f41cda64010b) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "ra2rom3.bin", 0x00000, 0x80000, CRC(eacd0cf7) SHA1(a04ddc339d330be4b278f12a54fe65d7eb08ffd0) )
	ROM_LOAD16_BYTE( "ra2rom4.bin", 0x00001, 0x80000, CRC(adac68d2) SHA1(6287a979a57004b1c4eea7f539550e23bac22904) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "ra2rom5.bin", 0x00000, 0x80000, CRC(ad9379a2) SHA1(93126969677cfce20e5b2e287662ac6b2ceee425) )
	ROM_LOAD16_BYTE( "ra2rom6.bin", 0x00001, 0x80000, CRC(12312874) SHA1(9d4d9d9fbec8536e8a003892643654d15f4535fa) )
ROM_END

ROM_START( robadv2c )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "r2ac17lt.bin", 0x00000, 0x40000, CRC(47ce9172) SHA1(e05be868c48e53f131936070abd350914f9befcf) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "rarom2.bin", 0x00000, 0x40000, CRC(092392cb) SHA1(fd52a0c4f46cb3242bf1b9e35ad5f41cda64010b) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "ra2rom3.bin", 0x00000, 0x80000, CRC(eacd0cf7) SHA1(a04ddc339d330be4b278f12a54fe65d7eb08ffd0) )
	ROM_LOAD16_BYTE( "ra2rom4.bin", 0x00001, 0x80000, CRC(adac68d2) SHA1(6287a979a57004b1c4eea7f539550e23bac22904) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "ra2rom5.bin", 0x00000, 0x80000, CRC(ad9379a2) SHA1(93126969677cfce20e5b2e287662ac6b2ceee425) )
	ROM_LOAD16_BYTE( "ra2rom6.bin", 0x00001, 0x80000, CRC(12312874) SHA1(9d4d9d9fbec8536e8a003892643654d15f4535fa) )
ROM_END

ROM_START( robadv2v1 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "r2av17sh.bin", 0x00000, 0x40000, CRC(fe4a3199) SHA1(d8c8f3d4e399e757b551748435ede1cb6a04ee3b) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "rarom2.bin", 0x00000, 0x40000, CRC(092392cb) SHA1(fd52a0c4f46cb3242bf1b9e35ad5f41cda64010b) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "ra2rom3.bin", 0x00000, 0x80000, CRC(eacd0cf7) SHA1(a04ddc339d330be4b278f12a54fe65d7eb08ffd0) )
	ROM_LOAD16_BYTE( "ra2rom4.bin", 0x00001, 0x80000, CRC(adac68d2) SHA1(6287a979a57004b1c4eea7f539550e23bac22904) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "ra2rom5.bin", 0x00000, 0x80000, CRC(ad9379a2) SHA1(93126969677cfce20e5b2e287662ac6b2ceee425) )
	ROM_LOAD16_BYTE( "ra2rom6.bin", 0x00001, 0x80000, CRC(12312874) SHA1(9d4d9d9fbec8536e8a003892643654d15f4535fa) )
ROM_END

ROM_START( robadv2v2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "r2av17r.bin", 0x00000, 0x40000, CRC(17350817) SHA1(5e1c978cd4cf0f319f49c366c3b7634500c873dd) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "rarom2.bin", 0x00000, 0x40000, CRC(092392cb) SHA1(fd52a0c4f46cb3242bf1b9e35ad5f41cda64010b) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "ra2rom3.bin", 0x00000, 0x80000, CRC(eacd0cf7) SHA1(a04ddc339d330be4b278f12a54fe65d7eb08ffd0) )
	ROM_LOAD16_BYTE( "ra2rom4.bin", 0x00001, 0x80000, CRC(adac68d2) SHA1(6287a979a57004b1c4eea7f539550e23bac22904) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "ra2rom5.bin", 0x00000, 0x80000, CRC(ad9379a2) SHA1(93126969677cfce20e5b2e287662ac6b2ceee425) )
	ROM_LOAD16_BYTE( "ra2rom6.bin", 0x00001, 0x80000, CRC(12312874) SHA1(9d4d9d9fbec8536e8a003892643654d15f4535fa) )
ROM_END

ROM_START( robadv2v3 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "r2av17lt.bin", 0x00000, 0x40000, CRC(0ebc91fe) SHA1(d64a29e05ce62d662eccb025ea905275eb8806f9) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "rarom2.bin", 0x00000, 0x40000, CRC(092392cb) SHA1(fd52a0c4f46cb3242bf1b9e35ad5f41cda64010b) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "ra2rom3.bin", 0x00000, 0x80000, CRC(eacd0cf7) SHA1(a04ddc339d330be4b278f12a54fe65d7eb08ffd0) )
	ROM_LOAD16_BYTE( "ra2rom4.bin", 0x00001, 0x80000, CRC(adac68d2) SHA1(6287a979a57004b1c4eea7f539550e23bac22904) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "ra2rom5.bin", 0x00000, 0x80000, CRC(ad9379a2) SHA1(93126969677cfce20e5b2e287662ac6b2ceee425) )
	ROM_LOAD16_BYTE( "ra2rom6.bin", 0x00001, 0x80000, CRC(12312874) SHA1(9d4d9d9fbec8536e8a003892643654d15f4535fa) )
ROM_END

ROM_START( robadv2v4 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "r2av17e.bin", 0x00000, 0x40000, CRC(81166cbd) SHA1(a2751752a95cac5181311af867457cac48854283) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "rarom2.bin", 0x00000, 0x40000, CRC(092392cb) SHA1(fd52a0c4f46cb3242bf1b9e35ad5f41cda64010b) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "ra2rom3.bin", 0x00000, 0x80000, CRC(eacd0cf7) SHA1(a04ddc339d330be4b278f12a54fe65d7eb08ffd0) )
	ROM_LOAD16_BYTE( "ra2rom4.bin", 0x00001, 0x80000, CRC(adac68d2) SHA1(6287a979a57004b1c4eea7f539550e23bac22904) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "ra2rom5.bin", 0x00000, 0x80000, CRC(ad9379a2) SHA1(93126969677cfce20e5b2e287662ac6b2ceee425) )
	ROM_LOAD16_BYTE( "ra2rom6.bin", 0x00001, 0x80000, CRC(12312874) SHA1(9d4d9d9fbec8536e8a003892643654d15f4535fa) )
ROM_END

ROM_START( robadv2d )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "r2ad17lt.bin", 0x00000, 0x40000, CRC(e0ea8ce9) SHA1(cf6a58d1cc654c41ae245f26fff6b26483bc01ce) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "rarom2.bin", 0x00000, 0x40000, CRC(092392cb) SHA1(fd52a0c4f46cb3242bf1b9e35ad5f41cda64010b) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "ra2rom3.bin", 0x00000, 0x80000, CRC(eacd0cf7) SHA1(a04ddc339d330be4b278f12a54fe65d7eb08ffd0) )
	ROM_LOAD16_BYTE( "ra2rom4.bin", 0x00001, 0x80000, CRC(adac68d2) SHA1(6287a979a57004b1c4eea7f539550e23bac22904) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "ra2rom5.bin", 0x00000, 0x80000, CRC(ad9379a2) SHA1(93126969677cfce20e5b2e287662ac6b2ceee425) )
	ROM_LOAD16_BYTE( "ra2rom6.bin", 0x00001, 0x80000, CRC(12312874) SHA1(9d4d9d9fbec8536e8a003892643654d15f4535fa) )
ROM_END

ROM_START( robadv2d2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "r2ad17r.bin", 0x00000, 0x40000, CRC(2454cd69) SHA1(51be4a4522176e23f1cd172d4fca5bcaf8802d1c) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "rarom2.bin", 0x00000, 0x40000, CRC(092392cb) SHA1(fd52a0c4f46cb3242bf1b9e35ad5f41cda64010b) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "ra2rom3.bin", 0x00000, 0x80000, CRC(eacd0cf7) SHA1(a04ddc339d330be4b278f12a54fe65d7eb08ffd0) )
	ROM_LOAD16_BYTE( "ra2rom4.bin", 0x00001, 0x80000, CRC(adac68d2) SHA1(6287a979a57004b1c4eea7f539550e23bac22904) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "ra2rom5.bin", 0x00000, 0x80000, CRC(ad9379a2) SHA1(93126969677cfce20e5b2e287662ac6b2ceee425) )
	ROM_LOAD16_BYTE( "ra2rom6.bin", 0x00001, 0x80000, CRC(12312874) SHA1(9d4d9d9fbec8536e8a003892643654d15f4535fa) )
ROM_END

ROM_START( robadv2d3 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "r2ad17sh.bin", 0x00000, 0x40000, CRC(d65ac29a) SHA1(74ae9ae2d3d2e6fb460bddf6adb676b778e42920) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "rarom2.bin", 0x00000, 0x40000, CRC(092392cb) SHA1(fd52a0c4f46cb3242bf1b9e35ad5f41cda64010b) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "ra2rom3.bin", 0x00000, 0x80000, CRC(eacd0cf7) SHA1(a04ddc339d330be4b278f12a54fe65d7eb08ffd0) )
	ROM_LOAD16_BYTE( "ra2rom4.bin", 0x00001, 0x80000, CRC(adac68d2) SHA1(6287a979a57004b1c4eea7f539550e23bac22904) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "ra2rom5.bin", 0x00000, 0x80000, CRC(ad9379a2) SHA1(93126969677cfce20e5b2e287662ac6b2ceee425) )
	ROM_LOAD16_BYTE( "ra2rom6.bin", 0x00001, 0x80000, CRC(12312874) SHA1(9d4d9d9fbec8536e8a003892643654d15f4535fa) )
ROM_END

ROM_START( robadv2c2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "r2ac17r.bin", 0x00000, 0x40000, CRC(92ad7e01) SHA1(47f41ed02b488e439f81936393618bde058ad661) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "rarom2.bin", 0x00000, 0x40000, CRC(092392cb) SHA1(fd52a0c4f46cb3242bf1b9e35ad5f41cda64010b) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "ra2rom3.bin", 0x00000, 0x80000, CRC(eacd0cf7) SHA1(a04ddc339d330be4b278f12a54fe65d7eb08ffd0) )
	ROM_LOAD16_BYTE( "ra2rom4.bin", 0x00001, 0x80000, CRC(adac68d2) SHA1(6287a979a57004b1c4eea7f539550e23bac22904) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "ra2rom5.bin", 0x00000, 0x80000, CRC(ad9379a2) SHA1(93126969677cfce20e5b2e287662ac6b2ceee425) )
	ROM_LOAD16_BYTE( "ra2rom6.bin", 0x00001, 0x80000, CRC(12312874) SHA1(9d4d9d9fbec8536e8a003892643654d15f4535fa) )
ROM_END

ROM_START( robadv2c3 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "r2ac17sh.bin", 0x00000, 0x40000, CRC(721fc8aa) SHA1(6985530f4dfb270f73f5c377b5831d5c2e087d05) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "rarom2.bin", 0x00000, 0x40000, CRC(092392cb) SHA1(fd52a0c4f46cb3242bf1b9e35ad5f41cda64010b) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "ra2rom3.bin", 0x00000, 0x80000, CRC(eacd0cf7) SHA1(a04ddc339d330be4b278f12a54fe65d7eb08ffd0) )
	ROM_LOAD16_BYTE( "ra2rom4.bin", 0x00001, 0x80000, CRC(adac68d2) SHA1(6287a979a57004b1c4eea7f539550e23bac22904) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "ra2rom5.bin", 0x00000, 0x80000, CRC(ad9379a2) SHA1(93126969677cfce20e5b2e287662ac6b2ceee425) )
	ROM_LOAD16_BYTE( "ra2rom6.bin", 0x00001, 0x80000, CRC(12312874) SHA1(9d4d9d9fbec8536e8a003892643654d15f4535fa) )
ROM_END


ROM_START( pirpok2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "p3p20.bin", 0x00000, 0x40000, CRC(0e477094) SHA1(cd35c9ac1ed4b843886b1fc554e749f38573ca21) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "p3rom2.bin", 0x00000, 0x20000, CRC(db6182e4) SHA1(65f05247629d5a1f37bf179f468acf8420342d2c) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "p3rom3.bin", 0x00000, 0x80000, CRC(34d3e6c5) SHA1(1d89677605188f135c8dbbc2ab20510cae7548fe) )
	ROM_LOAD16_BYTE( "p3rom4.bin", 0x00001, 0x80000, CRC(3861b5fb) SHA1(72f085a1fd951919f479e5c6984304f7bbddc054) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "p3rom5.bin", 0x00000, 0x80000, CRC(c5eca135) SHA1(bdaccd32e1434016c77579bc8c4214ab2a3ae474) )
	ROM_LOAD16_BYTE( "p3rom6.bin", 0x00001, 0x80000, CRC(d990cbb8) SHA1(6f822e38bf401b2eb0b2e36f3b4fc6822fafd3fa) )
ROM_END

ROM_START( pirpok2b )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "p3pb22r.bin", 0x00000, 0x40000, CRC(39303a7a) SHA1(ef4f1a01812818fe0f9fa5a23396094144c3ce83) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "p3rom2.bin", 0x00000, 0x20000, CRC(db6182e4) SHA1(65f05247629d5a1f37bf179f468acf8420342d2c) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "p3rom3.bin", 0x00000, 0x80000, CRC(34d3e6c5) SHA1(1d89677605188f135c8dbbc2ab20510cae7548fe) )
	ROM_LOAD16_BYTE( "p3rom4.bin", 0x00001, 0x80000, CRC(3861b5fb) SHA1(72f085a1fd951919f479e5c6984304f7bbddc054) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "p3rom5.bin", 0x00000, 0x80000, CRC(c5eca135) SHA1(bdaccd32e1434016c77579bc8c4214ab2a3ae474) )
	ROM_LOAD16_BYTE( "p3rom6.bin", 0x00001, 0x80000, CRC(d990cbb8) SHA1(6f822e38bf401b2eb0b2e36f3b4fc6822fafd3fa) )
ROM_END

ROM_START( pirpok2d )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "p3pd22r.bin", 0x00000, 0x40000, CRC(10262317) SHA1(561088d1ace055cd568d667f690e95fc9ee3fed3) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "p3rom2.bin", 0x00000, 0x20000, CRC(db6182e4) SHA1(65f05247629d5a1f37bf179f468acf8420342d2c) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "p3rom3.bin", 0x00000, 0x80000, CRC(34d3e6c5) SHA1(1d89677605188f135c8dbbc2ab20510cae7548fe) )
	ROM_LOAD16_BYTE( "p3rom4.bin", 0x00001, 0x80000, CRC(3861b5fb) SHA1(72f085a1fd951919f479e5c6984304f7bbddc054) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "p3rom5.bin", 0x00000, 0x80000, CRC(c5eca135) SHA1(bdaccd32e1434016c77579bc8c4214ab2a3ae474) )
	ROM_LOAD16_BYTE( "p3rom6.bin", 0x00001, 0x80000, CRC(d990cbb8) SHA1(6f822e38bf401b2eb0b2e36f3b4fc6822fafd3fa) )
ROM_END

ROM_START( pirpok2v )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "p3pv22r.bin", 0x00000, 0x40000, CRC(6e2aab96) SHA1(0e01c9cadcf947d68fab8626454ac06e2073b0e6) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "p3rom2.bin", 0x00000, 0x20000, CRC(db6182e4) SHA1(65f05247629d5a1f37bf179f468acf8420342d2c) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "p3rom3.bin", 0x00000, 0x80000, CRC(34d3e6c5) SHA1(1d89677605188f135c8dbbc2ab20510cae7548fe) )
	ROM_LOAD16_BYTE( "p3rom4.bin", 0x00001, 0x80000, CRC(3861b5fb) SHA1(72f085a1fd951919f479e5c6984304f7bbddc054) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "p3rom5.bin", 0x00000, 0x80000, CRC(c5eca135) SHA1(bdaccd32e1434016c77579bc8c4214ab2a3ae474) )
	ROM_LOAD16_BYTE( "p3rom6.bin", 0x00001, 0x80000, CRC(d990cbb8) SHA1(6f822e38bf401b2eb0b2e36f3b4fc6822fafd3fa) )
ROM_END

ROM_START( pirpok2v2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "p3pv24e.bin", 0x00000, 0x40000, CRC(0e77fb66) SHA1(732f9c160682dcfb6839c0ad28dfe7e4899e693c) )

	ROM_REGION( 0x040000, "oki", 0 ) /* Samples */
	ROM_LOAD( "p3rom2.bin", 0x00000, 0x20000, CRC(db6182e4) SHA1(65f05247629d5a1f37bf179f468acf8420342d2c) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "p3rom3.bin", 0x00000, 0x80000, CRC(34d3e6c5) SHA1(1d89677605188f135c8dbbc2ab20510cae7548fe) )
	ROM_LOAD16_BYTE( "p3rom4.bin", 0x00001, 0x80000, CRC(3861b5fb) SHA1(72f085a1fd951919f479e5c6984304f7bbddc054) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "p3rom5.bin", 0x00000, 0x80000, CRC(c5eca135) SHA1(bdaccd32e1434016c77579bc8c4214ab2a3ae474) )
	ROM_LOAD16_BYTE( "p3rom6.bin", 0x00001, 0x80000, CRC(d990cbb8) SHA1(6f822e38bf401b2eb0b2e36f3b4fc6822fafd3fa) )
ROM_END



ROM_START( fcnudge )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fc17n.bin", 0x00000, 0x40000, CRC(b9193d4f) SHA1(5ed77802e5a8f246eb1a559c13ad544adae35201) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "abrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fcrom3n.bin", 0x00000, 0x80000, CRC(3d1c3d7f) SHA1(bcb20c08a0a2a36775052ae45258862afc00d61d) )
	ROM_LOAD16_BYTE( "fcrom4n.bin", 0x00001, 0x80000, CRC(a047861e) SHA1(b5d160c25945c7c103160e80d545cb3e1091e631) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fcrom5n.bin", 0x00000, 0x80000, CRC(42955842) SHA1(383be3049da5b10ea57a278bc6578ece046058fd) )
	ROM_LOAD16_BYTE( "fcrom6n.bin", 0x00001, 0x80000, CRC(eee0f84d) SHA1(4ac096ccea258710f58c8121e7f0af28593d6368) )
ROM_END

ROM_START( fcnudge2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fcv21n.bin", 0x00000, 0x40000, CRC(c1f839bd) SHA1(12da36b864adcabe0386dc2e17dd9550fb23b641) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "abrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fcrom3n.bin", 0x00000, 0x80000, CRC(3d1c3d7f) SHA1(bcb20c08a0a2a36775052ae45258862afc00d61d) )
	ROM_LOAD16_BYTE( "fcrom4n.bin", 0x00001, 0x80000, CRC(a047861e) SHA1(b5d160c25945c7c103160e80d545cb3e1091e631) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fcrom5n.bin", 0x00000, 0x80000, CRC(42955842) SHA1(383be3049da5b10ea57a278bc6578ece046058fd) )
	ROM_LOAD16_BYTE( "fcrom6n.bin", 0x00001, 0x80000, CRC(eee0f84d) SHA1(4ac096ccea258710f58c8121e7f0af28593d6368) )
ROM_END

ROM_START( fcnudge3 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fcb20n.bin", 0x00000, 0x40000, CRC(f8de6fe2) SHA1(ff47b3f467e701897471b6aa912c086019d9ee6a) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "abrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fcrom3n.bin", 0x00000, 0x80000, CRC(3d1c3d7f) SHA1(bcb20c08a0a2a36775052ae45258862afc00d61d) )
	ROM_LOAD16_BYTE( "fcrom4n.bin", 0x00001, 0x80000, CRC(a047861e) SHA1(b5d160c25945c7c103160e80d545cb3e1091e631) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fcrom5n.bin", 0x00000, 0x80000, CRC(42955842) SHA1(383be3049da5b10ea57a278bc6578ece046058fd) )
	ROM_LOAD16_BYTE( "fcrom6n.bin", 0x00001, 0x80000, CRC(eee0f84d) SHA1(4ac096ccea258710f58c8121e7f0af28593d6368) )
ROM_END

ROM_START( fcnudge4 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fcd20n.bin", 0x00000, 0x40000, CRC(64c6a5cc) SHA1(dadc22ef7c2415c269619f63bca7761775eacf74) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "abrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fcrom3n.bin", 0x00000, 0x80000, CRC(3d1c3d7f) SHA1(bcb20c08a0a2a36775052ae45258862afc00d61d) )
	ROM_LOAD16_BYTE( "fcrom4n.bin", 0x00001, 0x80000, CRC(a047861e) SHA1(b5d160c25945c7c103160e80d545cb3e1091e631) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fcrom5n.bin", 0x00000, 0x80000, CRC(42955842) SHA1(383be3049da5b10ea57a278bc6578ece046058fd) )
	ROM_LOAD16_BYTE( "fcrom6n.bin", 0x00001, 0x80000, CRC(eee0f84d) SHA1(4ac096ccea258710f58c8121e7f0af28593d6368) )
ROM_END

ROM_START( sfruitb )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "sfb20b.bin", 0x00000, 0x40000, CRC(6fe1b8ba) SHA1(46fe3940d80578f3818702fd449fc4119ea5fc30) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "sfbrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "sfbrom3.bin", 0x00000, 0x80000, CRC(b48eb491) SHA1(0369873231ffa3fb78863623209ad1e05222fc8a) )
	ROM_LOAD16_BYTE( "sfbrom4.bin", 0x00001, 0x80000, CRC(a307119c) SHA1(b45a0e73d4e2d665de634dbf0034b3dcc9152b3d) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "sfbrom5.bin", 0x00000, 0x80000, CRC(31588ff3) SHA1(6e2a65d50457ec0e93a647fd8ca5ebeeb16bdb1c) )
	ROM_LOAD16_BYTE( "sfbrom6.bin", 0x00001, 0x80000, CRC(232d6216) SHA1(ca7780adc85fa570698736785ad700797e6a98fb) )
ROM_END

ROM_START( sfruitb2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "sfb20.bin", 0x00000, 0x40000, CRC(73a2be7f) SHA1(95b51a63ede10247fde944d980d85781947a8435) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "sfbrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "sfbrom3.bin", 0x00000, 0x80000, CRC(b48eb491) SHA1(0369873231ffa3fb78863623209ad1e05222fc8a) )
	ROM_LOAD16_BYTE( "sfbrom4.bin", 0x00001, 0x80000, CRC(a307119c) SHA1(b45a0e73d4e2d665de634dbf0034b3dcc9152b3d) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "sfbrom5.bin", 0x00000, 0x80000, CRC(31588ff3) SHA1(6e2a65d50457ec0e93a647fd8ca5ebeeb16bdb1c) )
	ROM_LOAD16_BYTE( "sfbrom6.bin", 0x00001, 0x80000, CRC(232d6216) SHA1(ca7780adc85fa570698736785ad700797e6a98fb) )
ROM_END

ROM_START( sfruitb3 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "sfb18xt.bin", 0x00000, 0x40000, CRC(15a7fc47) SHA1(4f1af0bab7807a69f8c67c8e83b35c8c5c2a13f1) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "sfbrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "sfbrom3.bin", 0x00000, 0x80000, CRC(b48eb491) SHA1(0369873231ffa3fb78863623209ad1e05222fc8a) )
	ROM_LOAD16_BYTE( "sfbrom4.bin", 0x00001, 0x80000, CRC(a307119c) SHA1(b45a0e73d4e2d665de634dbf0034b3dcc9152b3d) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "sfbrom5.bin", 0x00000, 0x80000, CRC(31588ff3) SHA1(6e2a65d50457ec0e93a647fd8ca5ebeeb16bdb1c) )
	ROM_LOAD16_BYTE( "sfbrom6.bin", 0x00001, 0x80000, CRC(232d6216) SHA1(ca7780adc85fa570698736785ad700797e6a98fb) )
ROM_END

ROM_START( sfruitbb )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "sfbb20lt.bin", 0x00000, 0x40000, CRC(418fbd9e) SHA1(b78e788b7bad85ce8f8709f20dcded25be9dac01) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "sfbrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "sfbrom3.bin", 0x00000, 0x80000, CRC(b48eb491) SHA1(0369873231ffa3fb78863623209ad1e05222fc8a) )
	ROM_LOAD16_BYTE( "sfbrom4.bin", 0x00001, 0x80000, CRC(a307119c) SHA1(b45a0e73d4e2d665de634dbf0034b3dcc9152b3d) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "sfbrom5.bin", 0x00000, 0x80000, CRC(31588ff3) SHA1(6e2a65d50457ec0e93a647fd8ca5ebeeb16bdb1c) )
	ROM_LOAD16_BYTE( "sfbrom6.bin", 0x00001, 0x80000, CRC(232d6216) SHA1(ca7780adc85fa570698736785ad700797e6a98fb) )
ROM_END

ROM_START( sfruitbb2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "sfbb22b.bin", 0x00000, 0x40000,  CRC(16abe969) SHA1(97ca2f223fb16c1003544c7454e470a31f54b3b3) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "sfbrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "sfbrom3.bin", 0x00000, 0x80000, CRC(b48eb491) SHA1(0369873231ffa3fb78863623209ad1e05222fc8a) )
	ROM_LOAD16_BYTE( "sfbrom4.bin", 0x00001, 0x80000, CRC(a307119c) SHA1(b45a0e73d4e2d665de634dbf0034b3dcc9152b3d) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "sfbrom5.bin", 0x00000, 0x80000, CRC(31588ff3) SHA1(6e2a65d50457ec0e93a647fd8ca5ebeeb16bdb1c) )
	ROM_LOAD16_BYTE( "sfbrom6.bin", 0x00001, 0x80000, CRC(232d6216) SHA1(ca7780adc85fa570698736785ad700797e6a98fb) )
ROM_END

ROM_START( sfruitbb3 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "sfbb25r.bin", 0x00000, 0x40000, CRC(bcb51221) SHA1(6df07a52557d8305fec45c8a030141cb15204548) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "sfbrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "sfbrom3.bin", 0x00000, 0x80000, CRC(b48eb491) SHA1(0369873231ffa3fb78863623209ad1e05222fc8a) )
	ROM_LOAD16_BYTE( "sfbrom4.bin", 0x00001, 0x80000, CRC(a307119c) SHA1(b45a0e73d4e2d665de634dbf0034b3dcc9152b3d) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "sfbrom5.bin", 0x00000, 0x80000, CRC(31588ff3) SHA1(6e2a65d50457ec0e93a647fd8ca5ebeeb16bdb1c) )
	ROM_LOAD16_BYTE( "sfbrom6.bin", 0x00001, 0x80000, CRC(232d6216) SHA1(ca7780adc85fa570698736785ad700797e6a98fb) )
ROM_END

ROM_START( sfruitbd )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "sfbd20lt.bin", 0x00000, 0x40000, CRC(9d0ebc24) SHA1(790050a35f91e683a5e2c2231c6b861a05eba04a) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "sfbrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "sfbrom3.bin", 0x00000, 0x80000, CRC(b48eb491) SHA1(0369873231ffa3fb78863623209ad1e05222fc8a) )
	ROM_LOAD16_BYTE( "sfbrom4.bin", 0x00001, 0x80000, CRC(a307119c) SHA1(b45a0e73d4e2d665de634dbf0034b3dcc9152b3d) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "sfbrom5.bin", 0x00000, 0x80000, CRC(31588ff3) SHA1(6e2a65d50457ec0e93a647fd8ca5ebeeb16bdb1c) )
	ROM_LOAD16_BYTE( "sfbrom6.bin", 0x00001, 0x80000, CRC(232d6216) SHA1(ca7780adc85fa570698736785ad700797e6a98fb) )
ROM_END

ROM_START( sfruitbd2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "sfbd22b.bin", 0x00000, 0x40000,  CRC(065bb398) SHA1(dd3092729bca420cdd338749d9bd779970dcd1c7) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "sfbrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "sfbrom3.bin", 0x00000, 0x80000, CRC(b48eb491) SHA1(0369873231ffa3fb78863623209ad1e05222fc8a) )
	ROM_LOAD16_BYTE( "sfbrom4.bin", 0x00001, 0x80000, CRC(a307119c) SHA1(b45a0e73d4e2d665de634dbf0034b3dcc9152b3d) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "sfbrom5.bin", 0x00000, 0x80000, CRC(31588ff3) SHA1(6e2a65d50457ec0e93a647fd8ca5ebeeb16bdb1c) )
	ROM_LOAD16_BYTE( "sfbrom6.bin", 0x00001, 0x80000, CRC(232d6216) SHA1(ca7780adc85fa570698736785ad700797e6a98fb) )
ROM_END

ROM_START( sfruitbd3 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "sfbd25r.bin", 0x00000, 0x40000, CRC(bb7bee79) SHA1(c66e62df0996486bead90331b714e9aa62bd585f) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "sfbrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "sfbrom3.bin", 0x00000, 0x80000, CRC(b48eb491) SHA1(0369873231ffa3fb78863623209ad1e05222fc8a) )
	ROM_LOAD16_BYTE( "sfbrom4.bin", 0x00001, 0x80000, CRC(a307119c) SHA1(b45a0e73d4e2d665de634dbf0034b3dcc9152b3d) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "sfbrom5.bin", 0x00000, 0x80000, CRC(31588ff3) SHA1(6e2a65d50457ec0e93a647fd8ca5ebeeb16bdb1c) )
	ROM_LOAD16_BYTE( "sfbrom6.bin", 0x00001, 0x80000, CRC(232d6216) SHA1(ca7780adc85fa570698736785ad700797e6a98fb) )
ROM_END


ROM_START( sfruitbv )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "sfbv25r.bin", 0x00000, 0x40000, CRC(beb1ee59) SHA1(d6f72d66085309f33965640b25c788657eee01e1) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "sfbrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "sfbrom3.bin", 0x00000, 0x80000, CRC(b48eb491) SHA1(0369873231ffa3fb78863623209ad1e05222fc8a) )
	ROM_LOAD16_BYTE( "sfbrom4.bin", 0x00001, 0x80000, CRC(a307119c) SHA1(b45a0e73d4e2d665de634dbf0034b3dcc9152b3d) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "sfbrom5.bin", 0x00000, 0x80000, CRC(31588ff3) SHA1(6e2a65d50457ec0e93a647fd8ca5ebeeb16bdb1c) )
	ROM_LOAD16_BYTE( "sfbrom6.bin", 0x00001, 0x80000, CRC(232d6216) SHA1(ca7780adc85fa570698736785ad700797e6a98fb) )
ROM_END

ROM_START( sfruitbv2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "sfbv25e.bin", 0x00000, 0x40000, CRC(a9c7edba) SHA1(f860b1077a9a12ff49e2dea0aac888e210787327) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "sfbrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "sfbrom3.bin", 0x00000, 0x80000, CRC(b48eb491) SHA1(0369873231ffa3fb78863623209ad1e05222fc8a) )
	ROM_LOAD16_BYTE( "sfbrom4.bin", 0x00001, 0x80000, CRC(a307119c) SHA1(b45a0e73d4e2d665de634dbf0034b3dcc9152b3d) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "sfbrom5.bin", 0x00000, 0x80000, CRC(31588ff3) SHA1(6e2a65d50457ec0e93a647fd8ca5ebeeb16bdb1c) )
	ROM_LOAD16_BYTE( "sfbrom6.bin", 0x00001, 0x80000, CRC(232d6216) SHA1(ca7780adc85fa570698736785ad700797e6a98fb) )
ROM_END

ROM_START( sfruitbv3 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "sfbv22eb.bin", 0x00000, 0x40000,  CRC(64d31a39) SHA1(cd2fc75b8d16e444796c52255de298b3b52e40e6) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "sfbrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "sfbrom3.bin", 0x00000, 0x80000, CRC(b48eb491) SHA1(0369873231ffa3fb78863623209ad1e05222fc8a) )
	ROM_LOAD16_BYTE( "sfbrom4.bin", 0x00001, 0x80000, CRC(a307119c) SHA1(b45a0e73d4e2d665de634dbf0034b3dcc9152b3d) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "sfbrom5.bin", 0x00000, 0x80000, CRC(31588ff3) SHA1(6e2a65d50457ec0e93a647fd8ca5ebeeb16bdb1c) )
	ROM_LOAD16_BYTE( "sfbrom6.bin", 0x00001, 0x80000, CRC(232d6216) SHA1(ca7780adc85fa570698736785ad700797e6a98fb) )
ROM_END

ROM_START( sfruitbv4 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "sfbv22b.bin", 0x00000, 0x40000,  CRC(ec0e8486) SHA1(249b8ecada6b7c0b3e16baa614620af80d7d8c6e) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "sfbrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "sfbrom3.bin", 0x00000, 0x80000, CRC(b48eb491) SHA1(0369873231ffa3fb78863623209ad1e05222fc8a) )
	ROM_LOAD16_BYTE( "sfbrom4.bin", 0x00001, 0x80000, CRC(a307119c) SHA1(b45a0e73d4e2d665de634dbf0034b3dcc9152b3d) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "sfbrom5.bin", 0x00000, 0x80000, CRC(31588ff3) SHA1(6e2a65d50457ec0e93a647fd8ca5ebeeb16bdb1c) )
	ROM_LOAD16_BYTE( "sfbrom6.bin", 0x00001, 0x80000, CRC(232d6216) SHA1(ca7780adc85fa570698736785ad700797e6a98fb) )
ROM_END

ROM_START( sfruitbv5 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "sfbv20lt.bin", 0x00000, 0x40000, CRC(63560472) SHA1(14446f2d8fd0314ca00478159cbb0507ac096e34) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "sfbrom2.bin", 0x00000, 0x20000, CRC(3823a36e) SHA1(4136e380b63546b9490033ad26d776f326eb9290) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "sfbrom3.bin", 0x00000, 0x80000, CRC(b48eb491) SHA1(0369873231ffa3fb78863623209ad1e05222fc8a) )
	ROM_LOAD16_BYTE( "sfbrom4.bin", 0x00001, 0x80000, CRC(a307119c) SHA1(b45a0e73d4e2d665de634dbf0034b3dcc9152b3d) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "sfbrom5.bin", 0x00000, 0x80000, CRC(31588ff3) SHA1(6e2a65d50457ec0e93a647fd8ca5ebeeb16bdb1c) )
	ROM_LOAD16_BYTE( "sfbrom6.bin", 0x00001, 0x80000, CRC(232d6216) SHA1(ca7780adc85fa570698736785ad700797e6a98fb) )
ROM_END


ROM_START( fb2gen )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fb2g16xt.bin", 0x00000, 0x40000, CRC(ea525ebb) SHA1(965bba045ba69ac4316b27d0d69b130119f9ce04) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb2grom2.bin", 0x00000, 0x40000, CRC(1cbbd43a) SHA1(6e31c3bdd677d9d3cb445294cf17a0efcb16d4ed) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb2grom3_older.bin", 0x00000, 0x80000, CRC(a4f33c67) SHA1(ec7f539725b2684add019c1dad3f230b5c798daa) )
	ROM_LOAD16_BYTE( "fb2grom4_older.bin", 0x00001, 0x80000, CRC(c142f2af) SHA1(3323de8cd09b64c1c8ccf51acf74444e577fdfb3) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb2grom5_older.bin", 0x00000, 0x80000, CRC(1c4172a8) SHA1(c45a57cd799681d442de02f8f07dbd9751929ca4) )
	ROM_LOAD16_BYTE( "fb2grom6_older.bin", 0x00001, 0x80000, CRC(953fdcc4) SHA1(c57e2b4a8273e789b96d39fe28d02bec5359b5f4) )
ROM_END

ROM_START( fb2gena )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fb2g15r.bin", 0x00000, 0x40000, CRC(a8daf67d) SHA1(6e980748ec77c4842676f14ffffe3f630879e9d9) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb2grom2.bin", 0x00000, 0x40000, CRC(1cbbd43a) SHA1(6e31c3bdd677d9d3cb445294cf17a0efcb16d4ed) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb2grom3_older.bin", 0x00000, 0x80000, CRC(a4f33c67) SHA1(ec7f539725b2684add019c1dad3f230b5c798daa) )
	ROM_LOAD16_BYTE( "fb2grom4_older.bin", 0x00001, 0x80000, CRC(c142f2af) SHA1(3323de8cd09b64c1c8ccf51acf74444e577fdfb3) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb2grom5_older.bin", 0x00000, 0x80000, CRC(1c4172a8) SHA1(c45a57cd799681d442de02f8f07dbd9751929ca4) )
	ROM_LOAD16_BYTE( "fb2grom6_older.bin", 0x00001, 0x80000, CRC(953fdcc4) SHA1(c57e2b4a8273e789b96d39fe28d02bec5359b5f4) )
ROM_END


ROM_START( fb2genc )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "f2gc18r.bin", 0x00000, 0x40000, CRC(f0adc7a4) SHA1(109490212d8c0bd25d6beb271939a83c06e468c6) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb2grom2.bin", 0x00000, 0x40000, CRC(1cbbd43a) SHA1(6e31c3bdd677d9d3cb445294cf17a0efcb16d4ed) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb2grom3.bin", 0x00000, 0x80000, CRC(21ade753) SHA1(ca70ab941740983626f4d274aa4a9edea366f38a) )
	ROM_LOAD16_BYTE( "fb2grom4.bin", 0x00001, 0x80000, CRC(56605a08) SHA1(09022dc797dd824a973c5126cafe7b086a94184c) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb2grom5.bin", 0x00000, 0x80000, CRC(6aa1e45d) SHA1(a821c98513ad851f5f9e2452620feb662c28f8bb) )
	ROM_LOAD16_BYTE( "fb2grom6.bin", 0x00001, 0x80000, CRC(1a525dcf) SHA1(20b1b2d6bdb0953300a6d9937b582fd5e20931ed) )
ROM_END

ROM_START( fb2genc2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "f2gc18lt.bin", 0x00000, 0x40000, CRC(d22f7e92) SHA1(8e2a8554bcb2e8f86d6d43672e7e4535ee4f89cf) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb2grom2.bin", 0x00000, 0x40000, CRC(1cbbd43a) SHA1(6e31c3bdd677d9d3cb445294cf17a0efcb16d4ed) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb2grom3.bin", 0x00000, 0x80000, CRC(21ade753) SHA1(ca70ab941740983626f4d274aa4a9edea366f38a) )
	ROM_LOAD16_BYTE( "fb2grom4.bin", 0x00001, 0x80000, CRC(56605a08) SHA1(09022dc797dd824a973c5126cafe7b086a94184c) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb2grom5.bin", 0x00000, 0x80000, CRC(6aa1e45d) SHA1(a821c98513ad851f5f9e2452620feb662c28f8bb) )
	ROM_LOAD16_BYTE( "fb2grom6.bin", 0x00001, 0x80000, CRC(1a525dcf) SHA1(20b1b2d6bdb0953300a6d9937b582fd5e20931ed) )
ROM_END

ROM_START( fb2genv )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "f2gv18r.bin", 0x00000, 0x40000, CRC(c827362b) SHA1(3a407d8f009666cc80d1588d034ed135e18ec34b) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb2grom2.bin", 0x00000, 0x40000, CRC(1cbbd43a) SHA1(6e31c3bdd677d9d3cb445294cf17a0efcb16d4ed) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb2grom3.bin", 0x00000, 0x80000, CRC(21ade753) SHA1(ca70ab941740983626f4d274aa4a9edea366f38a) )
	ROM_LOAD16_BYTE( "fb2grom4.bin", 0x00001, 0x80000, CRC(56605a08) SHA1(09022dc797dd824a973c5126cafe7b086a94184c) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb2grom5.bin", 0x00000, 0x80000, CRC(6aa1e45d) SHA1(a821c98513ad851f5f9e2452620feb662c28f8bb) )
	ROM_LOAD16_BYTE( "fb2grom6.bin", 0x00001, 0x80000, CRC(1a525dcf) SHA1(20b1b2d6bdb0953300a6d9937b582fd5e20931ed) )
ROM_END

ROM_START( fb2genv2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "f2gv18lt.bin", 0x00000, 0x40000, CRC(d2b45ef3) SHA1(e058004d042aac6dde67f0e7f924d204965b3b72) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb2grom2.bin", 0x00000, 0x40000, CRC(1cbbd43a) SHA1(6e31c3bdd677d9d3cb445294cf17a0efcb16d4ed) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb2grom3.bin", 0x00000, 0x80000, CRC(21ade753) SHA1(ca70ab941740983626f4d274aa4a9edea366f38a) )
	ROM_LOAD16_BYTE( "fb2grom4.bin", 0x00001, 0x80000, CRC(56605a08) SHA1(09022dc797dd824a973c5126cafe7b086a94184c) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb2grom5.bin", 0x00000, 0x80000, CRC(6aa1e45d) SHA1(a821c98513ad851f5f9e2452620feb662c28f8bb) )
	ROM_LOAD16_BYTE( "fb2grom6.bin", 0x00001, 0x80000, CRC(1a525dcf) SHA1(20b1b2d6bdb0953300a6d9937b582fd5e20931ed) )
ROM_END

ROM_START( fb2genv3 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "f2gv18e.bin", 0x00000, 0x40000, CRC(a24059c0) SHA1(e9bcf506a82e35a8c69f20fa700dd5e7025d56c2) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb2grom2.bin", 0x00000, 0x40000, CRC(1cbbd43a) SHA1(6e31c3bdd677d9d3cb445294cf17a0efcb16d4ed) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb2grom3.bin", 0x00000, 0x80000, CRC(21ade753) SHA1(ca70ab941740983626f4d274aa4a9edea366f38a) )
	ROM_LOAD16_BYTE( "fb2grom4.bin", 0x00001, 0x80000, CRC(56605a08) SHA1(09022dc797dd824a973c5126cafe7b086a94184c) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb2grom5.bin", 0x00000, 0x80000, CRC(6aa1e45d) SHA1(a821c98513ad851f5f9e2452620feb662c28f8bb) )
	ROM_LOAD16_BYTE( "fb2grom6.bin", 0x00001, 0x80000, CRC(1a525dcf) SHA1(20b1b2d6bdb0953300a6d9937b582fd5e20931ed) )
ROM_END

ROM_START( fb2gend )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "f2gd18r.bin", 0x00000, 0x40000, CRC(6a97bc44) SHA1(ef1d611c009cb1f5ff674fa30413607e3fbcbc45) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb2grom2.bin", 0x00000, 0x40000, CRC(1cbbd43a) SHA1(6e31c3bdd677d9d3cb445294cf17a0efcb16d4ed) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb2grom3.bin", 0x00000, 0x80000, CRC(21ade753) SHA1(ca70ab941740983626f4d274aa4a9edea366f38a) )
	ROM_LOAD16_BYTE( "fb2grom4.bin", 0x00001, 0x80000, CRC(56605a08) SHA1(09022dc797dd824a973c5126cafe7b086a94184c) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb2grom5.bin", 0x00000, 0x80000, CRC(6aa1e45d) SHA1(a821c98513ad851f5f9e2452620feb662c28f8bb) )
	ROM_LOAD16_BYTE( "fb2grom6.bin", 0x00001, 0x80000, CRC(1a525dcf) SHA1(20b1b2d6bdb0953300a6d9937b582fd5e20931ed) )
ROM_END

ROM_START( fb2gend2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "f2gd18lt.bin", 0x00000, 0x40000, CRC(b9f7978b) SHA1(739f8000e589ecad50be072c5e90727e96b00765) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb2grom2.bin", 0x00000, 0x40000, CRC(1cbbd43a) SHA1(6e31c3bdd677d9d3cb445294cf17a0efcb16d4ed) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb2grom3.bin", 0x00000, 0x80000, CRC(21ade753) SHA1(ca70ab941740983626f4d274aa4a9edea366f38a) )
	ROM_LOAD16_BYTE( "fb2grom4.bin", 0x00001, 0x80000, CRC(56605a08) SHA1(09022dc797dd824a973c5126cafe7b086a94184c) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb2grom5.bin", 0x00000, 0x80000, CRC(6aa1e45d) SHA1(a821c98513ad851f5f9e2452620feb662c28f8bb) )
	ROM_LOAD16_BYTE( "fb2grom6.bin", 0x00001, 0x80000, CRC(1a525dcf) SHA1(20b1b2d6bdb0953300a6d9937b582fd5e20931ed) )
ROM_END

ROM_START( fb2nd )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fb2e15.bin", 0x00000, 0x40000, CRC(40a4bc95) SHA1(f84d8615e5a247a6db7792e54d236fbd5008d794) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb2grom2.bin", 0x00000, 0x40000, CRC(1cbbd43a) SHA1(6e31c3bdd677d9d3cb445294cf17a0efcb16d4ed) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb2erom3.bin", 0x00000, 0x80000, CRC(58201f71) SHA1(1e8d44105194a619d75d106cebcef783edc810f2) )
	ROM_LOAD16_BYTE( "fb2erom4.bin", 0x00001, 0x80000, CRC(4f8cb873) SHA1(7fae47e41abb8e3fffd584f9a5507168c06b8b6b) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb2erom5.bin", 0x00000, 0x80000, CRC(1bc55876) SHA1(d04ff7bf97145d45de943129bc9f3cbe27f4588e) )
	ROM_LOAD16_BYTE( "fb2erom6.bin", 0x00001, 0x80000, CRC(71b43f19) SHA1(a0b7f2b1968e6c083f9793f1249edb339422370d) )
ROM_END


ROM_START( fb2ndc )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "f2ec18r.bin", 0x00000, 0x40000, CRC(d993916c) SHA1(3ca93c42a6e6f7cfbd4bfbcd2375f66b66a066ca) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb2grom2.bin", 0x00000, 0x40000, CRC(1cbbd43a) SHA1(6e31c3bdd677d9d3cb445294cf17a0efcb16d4ed) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb2erom3.bin", 0x00000, 0x80000, CRC(58201f71) SHA1(1e8d44105194a619d75d106cebcef783edc810f2) )
	ROM_LOAD16_BYTE( "fb2erom4.bin", 0x00001, 0x80000, CRC(4f8cb873) SHA1(7fae47e41abb8e3fffd584f9a5507168c06b8b6b) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb2erom5.bin", 0x00000, 0x80000, CRC(1bc55876) SHA1(d04ff7bf97145d45de943129bc9f3cbe27f4588e) )
	ROM_LOAD16_BYTE( "fb2erom6.bin", 0x00001, 0x80000, CRC(71b43f19) SHA1(a0b7f2b1968e6c083f9793f1249edb339422370d) )
ROM_END

ROM_START( fb2ndc2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "f2ec18lt.bin", 0x00000, 0x40000, CRC(675e413d) SHA1(e15fc96a8be701a01e1154dfea2c7d24c8239215) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb2grom2.bin", 0x00000, 0x40000, CRC(1cbbd43a) SHA1(6e31c3bdd677d9d3cb445294cf17a0efcb16d4ed) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb2erom3.bin", 0x00000, 0x80000, CRC(58201f71) SHA1(1e8d44105194a619d75d106cebcef783edc810f2) )
	ROM_LOAD16_BYTE( "fb2erom4.bin", 0x00001, 0x80000, CRC(4f8cb873) SHA1(7fae47e41abb8e3fffd584f9a5507168c06b8b6b) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb2erom5.bin", 0x00000, 0x80000, CRC(1bc55876) SHA1(d04ff7bf97145d45de943129bc9f3cbe27f4588e) )
	ROM_LOAD16_BYTE( "fb2erom6.bin", 0x00001, 0x80000, CRC(71b43f19) SHA1(a0b7f2b1968e6c083f9793f1249edb339422370d) )
ROM_END

ROM_START( fb2ndv )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "f2ev18r.bin", 0x00000, 0x40000, CRC(22abfee6) SHA1(f5542042aa60238decc0c29553e682971744f535) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb2grom2.bin", 0x00000, 0x40000, CRC(1cbbd43a) SHA1(6e31c3bdd677d9d3cb445294cf17a0efcb16d4ed) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb2erom3.bin", 0x00000, 0x80000, CRC(58201f71) SHA1(1e8d44105194a619d75d106cebcef783edc810f2) )
	ROM_LOAD16_BYTE( "fb2erom4.bin", 0x00001, 0x80000, CRC(4f8cb873) SHA1(7fae47e41abb8e3fffd584f9a5507168c06b8b6b) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb2erom5.bin", 0x00000, 0x80000, CRC(1bc55876) SHA1(d04ff7bf97145d45de943129bc9f3cbe27f4588e) )
	ROM_LOAD16_BYTE( "fb2erom6.bin", 0x00001, 0x80000, CRC(71b43f19) SHA1(a0b7f2b1968e6c083f9793f1249edb339422370d) )
ROM_END

ROM_START( fb2ndv2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "f2ev18lt.bin", 0x00000, 0x40000, CRC(b59418b9) SHA1(8d45709176db09d052a26d57f41bc18d78632ad0) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb2grom2.bin", 0x00000, 0x40000, CRC(1cbbd43a) SHA1(6e31c3bdd677d9d3cb445294cf17a0efcb16d4ed) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb2erom3.bin", 0x00000, 0x80000, CRC(58201f71) SHA1(1e8d44105194a619d75d106cebcef783edc810f2) )
	ROM_LOAD16_BYTE( "fb2erom4.bin", 0x00001, 0x80000, CRC(4f8cb873) SHA1(7fae47e41abb8e3fffd584f9a5507168c06b8b6b) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb2erom5.bin", 0x00000, 0x80000, CRC(1bc55876) SHA1(d04ff7bf97145d45de943129bc9f3cbe27f4588e) )
	ROM_LOAD16_BYTE( "fb2erom6.bin", 0x00001, 0x80000, CRC(71b43f19) SHA1(a0b7f2b1968e6c083f9793f1249edb339422370d) )
ROM_END

ROM_START( fb2ndd )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "f2ed18r.bin", 0x00000, 0x40000, CRC(48a4dbcd) SHA1(e1a2163be6345983d05b1931b5619678f025d667) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb2grom2.bin", 0x00000, 0x40000, CRC(1cbbd43a) SHA1(6e31c3bdd677d9d3cb445294cf17a0efcb16d4ed) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb2erom3.bin", 0x00000, 0x80000, CRC(58201f71) SHA1(1e8d44105194a619d75d106cebcef783edc810f2) )
	ROM_LOAD16_BYTE( "fb2erom4.bin", 0x00001, 0x80000, CRC(4f8cb873) SHA1(7fae47e41abb8e3fffd584f9a5507168c06b8b6b) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb2erom5.bin", 0x00000, 0x80000, CRC(1bc55876) SHA1(d04ff7bf97145d45de943129bc9f3cbe27f4588e) )
	ROM_LOAD16_BYTE( "fb2erom6.bin", 0x00001, 0x80000, CRC(71b43f19) SHA1(a0b7f2b1968e6c083f9793f1249edb339422370d) )
ROM_END

ROM_START( fb2ndd2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "f2ed18lt.bin", 0x00000, 0x40000, CRC(3c469121) SHA1(0a694ff77dd2f797acf5889a8773bb798f64f11b) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb2grom2.bin", 0x00000, 0x40000, CRC(1cbbd43a) SHA1(6e31c3bdd677d9d3cb445294cf17a0efcb16d4ed) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb2erom3.bin", 0x00000, 0x80000, CRC(58201f71) SHA1(1e8d44105194a619d75d106cebcef783edc810f2) )
	ROM_LOAD16_BYTE( "fb2erom4.bin", 0x00001, 0x80000, CRC(4f8cb873) SHA1(7fae47e41abb8e3fffd584f9a5507168c06b8b6b) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb2erom5.bin", 0x00000, 0x80000, CRC(1bc55876) SHA1(d04ff7bf97145d45de943129bc9f3cbe27f4588e) )
	ROM_LOAD16_BYTE( "fb2erom6.bin", 0x00001, 0x80000, CRC(71b43f19) SHA1(a0b7f2b1968e6c083f9793f1249edb339422370d) )
ROM_END

ROM_START( fb4 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fb24k13t.bin", 0x00000, 0x40000, CRC(ef2407cf) SHA1(4bfb8cd738d576e482828529bca3031b55cc165d) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb4rom2.bin", 0x00000, 0x3ffff, BAD_DUMP CRC(bf49ba49) SHA1(eea40e34298f7fd98771f0869ef541c5e1514f2a) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb4rom3.bin", 0x00000, 0x80000, CRC(4176937d) SHA1(dbde944a154f648a86628a8165fa27032115c417) )
	ROM_LOAD16_BYTE( "fb4rom4.bin", 0x00001, 0x80000, CRC(f8c57041) SHA1(ca8f58e89d31563b363a78db89e2711402f3ba80) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb4rom5.bin", 0x00000, 0x80000, CRC(41ad506c) SHA1(19086ab859a60e5127af0e51381cbb9fda6de74a) )
	ROM_LOAD16_BYTE( "fb4rom6.bin", 0x00001, 0x80000, CRC(f6c07f3d) SHA1(709fe2a443fdd32a3f9ab9161d5321a01c0119bb) )
ROM_END

ROM_START( fb4a )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fb24k12b.bin", 0x00000, 0x40000, CRC(b238411c) SHA1(947a243141766583ce170e1f92769952281bf386) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb4rom2.bin", 0x00000, 0x3ffff, BAD_DUMP CRC(bf49ba49) SHA1(eea40e34298f7fd98771f0869ef541c5e1514f2a) )

	ROM_REGION( 0x040000, "debugram", ROMREGION_ERASE00 )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb4rom3.bin", 0x00000, 0x80000, CRC(4176937d) SHA1(dbde944a154f648a86628a8165fa27032115c417) )
	ROM_LOAD16_BYTE( "fb4rom4.bin", 0x00001, 0x80000, CRC(f8c57041) SHA1(ca8f58e89d31563b363a78db89e2711402f3ba80) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb4rom5.bin", 0x00000, 0x80000, CRC(41ad506c) SHA1(19086ab859a60e5127af0e51381cbb9fda6de74a) )
	ROM_LOAD16_BYTE( "fb4rom6.bin", 0x00001, 0x80000, CRC(f6c07f3d) SHA1(709fe2a443fdd32a3f9ab9161d5321a01c0119bb) )
ROM_END

ROM_START( fb4b )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fb4b15lt.bin", 0x00000, 0x40000, CRC(480651c3) SHA1(3ac434070b00c04eda9c78209e1c6e21fd488287) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb4rom2.bin", 0x00000, 0x3ffff, BAD_DUMP CRC(bf49ba49) SHA1(eea40e34298f7fd98771f0869ef541c5e1514f2a) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb4rom3.bin", 0x00000, 0x80000, CRC(4176937d) SHA1(dbde944a154f648a86628a8165fa27032115c417) )
	ROM_LOAD16_BYTE( "fb4rom4.bin", 0x00001, 0x80000, CRC(f8c57041) SHA1(ca8f58e89d31563b363a78db89e2711402f3ba80) )


	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb4rom5.bin", 0x00000, 0x80000, CRC(41ad506c) SHA1(19086ab859a60e5127af0e51381cbb9fda6de74a) )
	ROM_LOAD16_BYTE( "fb4rom6.bin", 0x00001, 0x80000, CRC(f6c07f3d) SHA1(709fe2a443fdd32a3f9ab9161d5321a01c0119bb) )
ROM_END

ROM_START( fb4b2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fb4b15r.bin", 0x00000, 0x40000, CRC(511a1c54) SHA1(7b554be602e74088ca4ab90a0b10965dc30b18ab) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb4rom2.bin", 0x00000, 0x3ffff, BAD_DUMP CRC(bf49ba49) SHA1(eea40e34298f7fd98771f0869ef541c5e1514f2a) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb4rom3.bin", 0x00000, 0x80000, CRC(4176937d) SHA1(dbde944a154f648a86628a8165fa27032115c417) )
	ROM_LOAD16_BYTE( "fb4rom4.bin", 0x00001, 0x80000, CRC(f8c57041) SHA1(ca8f58e89d31563b363a78db89e2711402f3ba80) )


	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb4rom5.bin", 0x00000, 0x80000, CRC(41ad506c) SHA1(19086ab859a60e5127af0e51381cbb9fda6de74a) )
	ROM_LOAD16_BYTE( "fb4rom6.bin", 0x00001, 0x80000, CRC(f6c07f3d) SHA1(709fe2a443fdd32a3f9ab9161d5321a01c0119bb) )

ROM_END

ROM_START( fb4v )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fb4v15r.bin", 0x00000, 0x40000, CRC(891f119f) SHA1(1823826cd958a951a930b9a1a23f7cf092ed6ab2) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb4rom2.bin", 0x00000, 0x3ffff, BAD_DUMP CRC(bf49ba49) SHA1(eea40e34298f7fd98771f0869ef541c5e1514f2a) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb4rom3.bin", 0x00000, 0x80000, CRC(4176937d) SHA1(dbde944a154f648a86628a8165fa27032115c417) )
	ROM_LOAD16_BYTE( "fb4rom4.bin", 0x00001, 0x80000, CRC(f8c57041) SHA1(ca8f58e89d31563b363a78db89e2711402f3ba80) )


	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb4rom5.bin", 0x00000, 0x80000, CRC(41ad506c) SHA1(19086ab859a60e5127af0e51381cbb9fda6de74a) )
	ROM_LOAD16_BYTE( "fb4rom6.bin", 0x00001, 0x80000, CRC(f6c07f3d) SHA1(709fe2a443fdd32a3f9ab9161d5321a01c0119bb) )

ROM_END

ROM_START( fb4v2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fb4v15lt.bin", 0x00000, 0x40000, CRC(d1cf9bd8) SHA1(59b1507e2d37eef8bea8d07194465506a52e7286))

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb4rom2.bin", 0x00000, 0x3ffff, BAD_DUMP CRC(bf49ba49) SHA1(eea40e34298f7fd98771f0869ef541c5e1514f2a) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb4rom3.bin", 0x00000, 0x80000, CRC(4176937d) SHA1(dbde944a154f648a86628a8165fa27032115c417) )
	ROM_LOAD16_BYTE( "fb4rom4.bin", 0x00001, 0x80000, CRC(f8c57041) SHA1(ca8f58e89d31563b363a78db89e2711402f3ba80) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb4rom5.bin", 0x00000, 0x80000, CRC(41ad506c) SHA1(19086ab859a60e5127af0e51381cbb9fda6de74a) )
	ROM_LOAD16_BYTE( "fb4rom6.bin", 0x00001, 0x80000, CRC(f6c07f3d) SHA1(709fe2a443fdd32a3f9ab9161d5321a01c0119bb) )
ROM_END

ROM_START( fb4v3 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fb4v15e.bin", 0x00000, 0x40000, CRC(b28db56e) SHA1(b14c0b62fc1c3195ee3703b5500f5a36a2cde3e2) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb4rom2.bin", 0x00000, 0x3ffff, BAD_DUMP CRC(bf49ba49) SHA1(eea40e34298f7fd98771f0869ef541c5e1514f2a) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb4rom3e.bin", 0x00000, 0x80000, CRC(d47d9969) SHA1(172771896b9ac75c34ae4c9958e26ba30371bdde) )
	ROM_LOAD16_BYTE( "fb4rom4e.bin", 0x00001, 0x80000, CRC(680fc5d1) SHA1(92d46b72584d2bc906901d7e7f44c017995ef2c0) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb4rom5e.bin", 0x00000, 0x80000, CRC(ddc02e07) SHA1(b1cce95ab09822646c835b066d4510a51633d107) )
	ROM_LOAD16_BYTE( "fb4rom6e.bin", 0x00001, 0x80000, CRC(e3de53a4) SHA1(3168ec7e10eee205655ee259fb5ba7201d7eb711) )
ROM_END

ROM_START( fb4d )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fb4d15r.bin", 0x00000, 0x40000, CRC(aeed6133) SHA1(8658708fbfd7f662f72a30a3f37baca98e931589) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb4rom2.bin", 0x00000, 0x3ffff, BAD_DUMP CRC(bf49ba49) SHA1(eea40e34298f7fd98771f0869ef541c5e1514f2a) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb4rom3.bin", 0x00000, 0x80000, CRC(4176937d) SHA1(dbde944a154f648a86628a8165fa27032115c417) )
	ROM_LOAD16_BYTE( "fb4rom4.bin", 0x00001, 0x80000, CRC(f8c57041) SHA1(ca8f58e89d31563b363a78db89e2711402f3ba80) )


	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb4rom5.bin", 0x00000, 0x80000, CRC(41ad506c) SHA1(19086ab859a60e5127af0e51381cbb9fda6de74a) )
	ROM_LOAD16_BYTE( "fb4rom6.bin", 0x00001, 0x80000, CRC(f6c07f3d) SHA1(709fe2a443fdd32a3f9ab9161d5321a01c0119bb) )
ROM_END

ROM_START( fb4d2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fb4d15lt.bin", 0x00000, 0x40000,  CRC(41b0177b) SHA1(9fc74f54a21fb2846e9f818e9b9714643cad0295) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb4rom2.bin", 0x00000, 0x3ffff, BAD_DUMP CRC(bf49ba49) SHA1(eea40e34298f7fd98771f0869ef541c5e1514f2a) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb4rom3.bin", 0x00000, 0x80000, CRC(4176937d) SHA1(dbde944a154f648a86628a8165fa27032115c417) )
	ROM_LOAD16_BYTE( "fb4rom4.bin", 0x00001, 0x80000, CRC(f8c57041) SHA1(ca8f58e89d31563b363a78db89e2711402f3ba80) )


	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb4rom5.bin", 0x00000, 0x80000, CRC(41ad506c) SHA1(19086ab859a60e5127af0e51381cbb9fda6de74a) )
	ROM_LOAD16_BYTE( "fb4rom6.bin", 0x00001, 0x80000, CRC(f6c07f3d) SHA1(709fe2a443fdd32a3f9ab9161d5321a01c0119bb) )
ROM_END

ROM_START( fb4c )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fb4c15r.bin", 0x00000, 0x40000, CRC(f50ce62f) SHA1(7a1c37f42da0506ff3bcebcd587f0105004b47e2) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb4rom2.bin", 0x00000, 0x3ffff, BAD_DUMP CRC(bf49ba49) SHA1(eea40e34298f7fd98771f0869ef541c5e1514f2a) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb4rom3.bin", 0x00000, 0x80000, CRC(4176937d) SHA1(dbde944a154f648a86628a8165fa27032115c417) )
	ROM_LOAD16_BYTE( "fb4rom4.bin", 0x00001, 0x80000, CRC(f8c57041) SHA1(ca8f58e89d31563b363a78db89e2711402f3ba80) )


	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb4rom5.bin", 0x00000, 0x80000, CRC(41ad506c) SHA1(19086ab859a60e5127af0e51381cbb9fda6de74a) )
	ROM_LOAD16_BYTE( "fb4rom6.bin", 0x00001, 0x80000, CRC(f6c07f3d) SHA1(709fe2a443fdd32a3f9ab9161d5321a01c0119bb) )
ROM_END

ROM_START( fb4c2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fb4c15lt.bin", 0x00000, 0x40000, CRC(280a0d31) SHA1(dba0dc3f14f08f8045934acd85cb549ca4292808) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb4rom2.bin", 0x00000, 0x3ffff, BAD_DUMP CRC(bf49ba49) SHA1(eea40e34298f7fd98771f0869ef541c5e1514f2a) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb4rom3.bin", 0x00000, 0x80000, CRC(4176937d) SHA1(dbde944a154f648a86628a8165fa27032115c417) )
	ROM_LOAD16_BYTE( "fb4rom4.bin", 0x00001, 0x80000, CRC(f8c57041) SHA1(ca8f58e89d31563b363a78db89e2711402f3ba80) )


	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb4rom5.bin", 0x00000, 0x80000, CRC(41ad506c) SHA1(19086ab859a60e5127af0e51381cbb9fda6de74a) )
	ROM_LOAD16_BYTE( "fb4rom6.bin", 0x00001, 0x80000, CRC(f6c07f3d) SHA1(709fe2a443fdd32a3f9ab9161d5321a01c0119bb) )
ROM_END

ROM_START( ch2000 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fc2k39xt.bin", 0x00000, 0x40000, CRC(fa330fdc) SHA1(8bafb76762ca64d5d4e16e4542585083078ce719) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fbrom2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fbrom3.bin", 0x00000, 0x40000, CRC(a712b521) SHA1(355b3bd892d5fbd360961ca4b5adb20ddf2ba553) )
	ROM_LOAD16_BYTE( "fbrom4.bin", 0x00001, 0x40000, CRC(8996d2d5) SHA1(fc3830b8126ef9d15108e0873209168ad0b608c8) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fbrom5.bin", 0x00000, 0x40000, CRC(494fd1fa) SHA1(01ca60e35b68da398612fc7c8a7da6f8835eabd5) )
	ROM_LOAD16_BYTE( "fbrom6.bin", 0x00001, 0x40000, CRC(0ebe2ea5) SHA1(d83c1ba940e43ce1d392969055f36b3c49ac9727) )
ROM_END

ROM_START( ch2000x )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fc2k39d.bin", 0x00000, 0x40000, CRC(38fa136c) SHA1(cae17a6340829f2d1963ffcd8fde89fdf9425a6b) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fbrom2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fbrom3.bin", 0x00000, 0x40000, CRC(a712b521) SHA1(355b3bd892d5fbd360961ca4b5adb20ddf2ba553) )
	ROM_LOAD16_BYTE( "fbrom4.bin", 0x00001, 0x40000, CRC(8996d2d5) SHA1(fc3830b8126ef9d15108e0873209168ad0b608c8) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fbrom5.bin", 0x00000, 0x40000, CRC(494fd1fa) SHA1(01ca60e35b68da398612fc7c8a7da6f8835eabd5) )
	ROM_LOAD16_BYTE( "fbrom6.bin", 0x00001, 0x40000, CRC(0ebe2ea5) SHA1(d83c1ba940e43ce1d392969055f36b3c49ac9727) )
ROM_END

ROM_START( ch2000y )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fc2k39.bin", 0x00000, 0x40000, CRC(77901459) SHA1(f30c416973550bf2598eb5ec388158d864ace089) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fbrom2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fbrom3.bin", 0x00000, 0x40000, CRC(a712b521) SHA1(355b3bd892d5fbd360961ca4b5adb20ddf2ba553) )
	ROM_LOAD16_BYTE( "fbrom4.bin", 0x00001, 0x40000, CRC(8996d2d5) SHA1(fc3830b8126ef9d15108e0873209168ad0b608c8) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fbrom5.bin", 0x00000, 0x40000, CRC(494fd1fa) SHA1(01ca60e35b68da398612fc7c8a7da6f8835eabd5) )
	ROM_LOAD16_BYTE( "fbrom6.bin", 0x00001, 0x40000, CRC(0ebe2ea5) SHA1(d83c1ba940e43ce1d392969055f36b3c49ac9727) )
ROM_END


/* b type */
ROM_START( ch2000b )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fc2b44r.bin", 0x00000, 0x40000, CRC(c9f9b0c7) SHA1(97bc35dcf0608c6211f1dc9678b4b2232c70cdca) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fbrom2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fbrom3.bin", 0x00000, 0x40000, CRC(a712b521) SHA1(355b3bd892d5fbd360961ca4b5adb20ddf2ba553) )
	ROM_LOAD16_BYTE( "fbrom4.bin", 0x00001, 0x40000, CRC(8996d2d5) SHA1(fc3830b8126ef9d15108e0873209168ad0b608c8) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fbrom5.bin", 0x00000, 0x40000, CRC(494fd1fa) SHA1(01ca60e35b68da398612fc7c8a7da6f8835eabd5) )
	ROM_LOAD16_BYTE( "fbrom6.bin", 0x00001, 0x40000, CRC(0ebe2ea5) SHA1(d83c1ba940e43ce1d392969055f36b3c49ac9727) )
ROM_END

ROM_START( ch2000b2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fc2b41lt.bin", 0x00000, 0x40000, CRC(0c8c40b0) SHA1(091fe168b0915940f7a15e33845dfd62c0a581df))

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fbrom2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fbrom3.bin", 0x00000, 0x40000, CRC(a712b521) SHA1(355b3bd892d5fbd360961ca4b5adb20ddf2ba553) )
	ROM_LOAD16_BYTE( "fbrom4.bin", 0x00001, 0x40000, CRC(8996d2d5) SHA1(fc3830b8126ef9d15108e0873209168ad0b608c8) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fbrom5.bin", 0x00000, 0x40000, CRC(494fd1fa) SHA1(01ca60e35b68da398612fc7c8a7da6f8835eabd5) )
	ROM_LOAD16_BYTE( "fbrom6.bin", 0x00001, 0x40000, CRC(0ebe2ea5) SHA1(d83c1ba940e43ce1d392969055f36b3c49ac9727) )
ROM_END

/* v type */
ROM_START( ch2000v )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fc2v44r.bin", 0x00000, 0x40000,  CRC(8d375e98) SHA1(29edfcd05e1759be2c7e92c3cb8f9929f8485715) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fbrom2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb2rom3.bin", 0x00000, 0x40000, CRC(e9032c12) SHA1(62d99452af8d89e46c202a87faed1c78042cc2f0) )
	ROM_LOAD16_BYTE( "fb2rom4.bin", 0x00001, 0x40000, CRC(fb019fcf) SHA1(943ca8cfeae786bf3fb52417578133fd5037f8e1) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb2rom5.bin", 0x00000, 0x40000, CRC(1dbf0566) SHA1(9b643cf03b543b9d7689daa00b2b06af5bc57c90) )
	ROM_LOAD16_BYTE( "fb2rom6.bin", 0x00001, 0x40000, CRC(a0116b86) SHA1(e2a0abbfbfa531683ea9077cdbed57d965f9c5c2) )
ROM_END

ROM_START( ch2000v2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fc2v44e.bin", 0x00000, 0x40000,  CRC(a9713624) SHA1(09bcecef4dec51ab573903e8652a3a7f6ae52e31) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fbrom2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb2rom3.bin", 0x00000, 0x40000, CRC(e9032c12) SHA1(62d99452af8d89e46c202a87faed1c78042cc2f0) )
	ROM_LOAD16_BYTE( "fb2rom4.bin", 0x00001, 0x40000, CRC(fb019fcf) SHA1(943ca8cfeae786bf3fb52417578133fd5037f8e1) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb2rom5.bin", 0x00000, 0x40000, CRC(1dbf0566) SHA1(9b643cf03b543b9d7689daa00b2b06af5bc57c90) )
	ROM_LOAD16_BYTE( "fb2rom6.bin", 0x00001, 0x40000, CRC(a0116b86) SHA1(e2a0abbfbfa531683ea9077cdbed57d965f9c5c2) )
ROM_END

ROM_START( ch2000v3 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fc2v41lt.bin", 0x00000, 0x40000, CRC(182ed2ff) SHA1(82df7021ec15fa2867f24292060d4a8089d5f49c) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fbrom2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb2rom3.bin", 0x00000, 0x40000, CRC(e9032c12) SHA1(62d99452af8d89e46c202a87faed1c78042cc2f0) )
	ROM_LOAD16_BYTE( "fb2rom4.bin", 0x00001, 0x40000, CRC(fb019fcf) SHA1(943ca8cfeae786bf3fb52417578133fd5037f8e1) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb2rom5.bin", 0x00000, 0x40000, CRC(1dbf0566) SHA1(9b643cf03b543b9d7689daa00b2b06af5bc57c90) )
	ROM_LOAD16_BYTE( "fb2rom6.bin", 0x00001, 0x40000, CRC(a0116b86) SHA1(e2a0abbfbfa531683ea9077cdbed57d965f9c5c2) )
ROM_END

ROM_START( ch2000c )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fc2c41lt.bin", 0x00000, 0x40000, CRC(bb6ddba8) SHA1(9f95cc35408f61f07ce0306fb41f3c31ec9ebe87) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fbrom2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb2rom3.bin", 0x00000, 0x40000, CRC(e9032c12) SHA1(62d99452af8d89e46c202a87faed1c78042cc2f0) )
	ROM_LOAD16_BYTE( "fb2rom4.bin", 0x00001, 0x40000, CRC(fb019fcf) SHA1(943ca8cfeae786bf3fb52417578133fd5037f8e1) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb2rom5.bin", 0x00000, 0x40000, CRC(1dbf0566) SHA1(9b643cf03b543b9d7689daa00b2b06af5bc57c90) )
	ROM_LOAD16_BYTE( "fb2rom6.bin", 0x00001, 0x40000, CRC(a0116b86) SHA1(e2a0abbfbfa531683ea9077cdbed57d965f9c5c2) )
ROM_END

ROM_START( ch2000c2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fc2c44r.bin", 0x00000, 0x40000,  CRC(d898129f) SHA1(1fdc35dd0332ecd705665db3b268e5d05f9d65dd) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fbrom2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb2rom3.bin", 0x00000, 0x40000, CRC(e9032c12) SHA1(62d99452af8d89e46c202a87faed1c78042cc2f0) )
	ROM_LOAD16_BYTE( "fb2rom4.bin", 0x00001, 0x40000, CRC(fb019fcf) SHA1(943ca8cfeae786bf3fb52417578133fd5037f8e1) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb2rom5.bin", 0x00000, 0x40000, CRC(1dbf0566) SHA1(9b643cf03b543b9d7689daa00b2b06af5bc57c90) )
	ROM_LOAD16_BYTE( "fb2rom6.bin", 0x00001, 0x40000, CRC(a0116b86) SHA1(e2a0abbfbfa531683ea9077cdbed57d965f9c5c2) )
ROM_END


ROM_START( ch2000d )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fc2d41lt.bin", 0x00000, 0x40000, CRC(d49d4303) SHA1(5e75e6d04ff96de212131fecf76c0e300b49b21d) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fbrom2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb2rom3.bin", 0x00000, 0x40000, CRC(e9032c12) SHA1(62d99452af8d89e46c202a87faed1c78042cc2f0) )
	ROM_LOAD16_BYTE( "fb2rom4.bin", 0x00001, 0x40000, CRC(fb019fcf) SHA1(943ca8cfeae786bf3fb52417578133fd5037f8e1) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb2rom5.bin", 0x00000, 0x40000, CRC(1dbf0566) SHA1(9b643cf03b543b9d7689daa00b2b06af5bc57c90) )
	ROM_LOAD16_BYTE( "fb2rom6.bin", 0x00001, 0x40000, CRC(a0116b86) SHA1(e2a0abbfbfa531683ea9077cdbed57d965f9c5c2) )
ROM_END

ROM_START( ch2000d2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fc2d44r.bin", 0x00000, 0x40000,  CRC(c00fd8c5) SHA1(f7977ec5797f2d20f21b018207808ab9d9d36d71) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fbrom2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb2rom3.bin", 0x00000, 0x40000, CRC(e9032c12) SHA1(62d99452af8d89e46c202a87faed1c78042cc2f0) )
	ROM_LOAD16_BYTE( "fb2rom4.bin", 0x00001, 0x40000, CRC(fb019fcf) SHA1(943ca8cfeae786bf3fb52417578133fd5037f8e1) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb2rom5.bin", 0x00000, 0x40000, CRC(1dbf0566) SHA1(9b643cf03b543b9d7689daa00b2b06af5bc57c90) )
	ROM_LOAD16_BYTE( "fb2rom6.bin", 0x00001, 0x40000, CRC(a0116b86) SHA1(e2a0abbfbfa531683ea9077cdbed57d965f9c5c2) )
ROM_END




/*

Action 2000 by AMCOE


Graphics - HM86171-80 ? 28DIP
Processor - Amcoe Saltire ? 208PQFP

QUARTZ OSCILLATORS 12.000, 4.9152 and 27.000

RAM

HM6264ALP-15 - 28DIP
UN61256FK-15 - 28DIP

Unknown

Chip Markings removed 44Pin PLCC

Roms

a2k1-1.2.u28 - F29C51001T
a2k-2.u11 - F29C51002T
a2k-3.u9 - F29C51004T
a2k-4.u8 - F29C51004T
a2k-5.u6 - F29C51004T
a2k-6.u4 - F29C51004T

Dumped by Dang_Spot 08/12/04
*/

ROM_START( act2000 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "a2k1-1.2.u28", 0x00000, 0x20000, CRC(ef9d7399) SHA1(8b4b7df85c4b0a22cb591be142bf8fea37c4b211) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "a2k-2.u11", 0x00000, 0x40000, CRC(5973b644) SHA1(428e4301e495000c3903c9e942d3dfba8261d745) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "a2k-3.u9", 0x00000, 0x80000, CRC(e91c51b0) SHA1(7858f30eb698ee37d27dd61a7df092000e8f7a7c) )
	ROM_LOAD16_BYTE( "a2k-4.u8", 0x00001, 0x80000, CRC(1238f1ae) SHA1(073df71dd13a77157ae9c94204cf69fda8286e0b) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "a2k-5.u6", 0x00000, 0x80000, CRC(2b4f7af8) SHA1(6892de184f0824d7b71c48b75db4dce19d230923) )
	ROM_LOAD16_BYTE( "a2k-6.u4", 0x00001, 0x80000, CRC(1b812dd6) SHA1(55998bd26ff9795087e6e240cc202306121920e8) )
ROM_END

ROM_START( act2000a )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "a2k33.bin", 0x00000, 0x20000, CRC(e096da60) SHA1(3e971ae152058c730a7ca35ce1ed3ce3896f34f5) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "a2klink2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "a2klink3.bin", 0x00000, 0x80000, CRC(10298268) SHA1(05b4c6ae90f069b67e7c17b7a74dc786888274a6) )
	ROM_LOAD16_BYTE( "a2klink4.bin", 0x00001, 0x80000, CRC(9c90cada) SHA1(10afbc7900ad876fddbe912c95bac1f575e0948e) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "a2klink5.bin", 0x00000, 0x80000, CRC(7083106a) SHA1(39e7da2ef91dda40b2a9d9b8d50c587d637fda54) )
	ROM_LOAD16_BYTE( "a2klink6.bin", 0x00001, 0x80000, CRC(ba0de415) SHA1(fb4d718b9ad95eaa0ca259605fc1f2916bd1b7b7) )
ROM_END

ROM_START( act2000a2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "a2k31xt.bin", 0x00000, 0x20000, CRC(46b3b809) SHA1(cbb88dda67fca89801c6db3bf0bf3a368fe26ad1) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "a2klink2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "a2klink3.bin", 0x00000, 0x80000, CRC(10298268) SHA1(05b4c6ae90f069b67e7c17b7a74dc786888274a6) )
	ROM_LOAD16_BYTE( "a2klink4.bin", 0x00001, 0x80000, CRC(9c90cada) SHA1(10afbc7900ad876fddbe912c95bac1f575e0948e) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "a2klink5.bin", 0x00000, 0x80000, CRC(7083106a) SHA1(39e7da2ef91dda40b2a9d9b8d50c587d637fda54) )
	ROM_LOAD16_BYTE( "a2klink6.bin", 0x00001, 0x80000, CRC(ba0de415) SHA1(fb4d718b9ad95eaa0ca259605fc1f2916bd1b7b7) )
ROM_END

ROM_START( act2000v )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "a2v35r.bin", 0x00000, 0x40000, CRC(e9651cea) SHA1(5717bf21e8b82f7d3e668235f189af2aaac9c425) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "a2klink2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "a2klink3.bin", 0x00000, 0x80000, CRC(10298268) SHA1(05b4c6ae90f069b67e7c17b7a74dc786888274a6) )
	ROM_LOAD16_BYTE( "a2klink4.bin", 0x00001, 0x80000, CRC(9c90cada) SHA1(10afbc7900ad876fddbe912c95bac1f575e0948e) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "a2klink5.bin", 0x00000, 0x80000, CRC(7083106a) SHA1(39e7da2ef91dda40b2a9d9b8d50c587d637fda54) )
	ROM_LOAD16_BYTE( "a2klink6.bin", 0x00001, 0x80000, CRC(ba0de415) SHA1(fb4d718b9ad95eaa0ca259605fc1f2916bd1b7b7) )
ROM_END

ROM_START( act2000v2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "a2v35e.bin", 0x00000, 0x40000, CRC(dfe5c8b5) SHA1(09ac6df25395d0a5c632c05ba93bf784b69319a0) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "a2klink2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "a2klink3.bin", 0x00000, 0x80000, CRC(10298268) SHA1(05b4c6ae90f069b67e7c17b7a74dc786888274a6) )
	ROM_LOAD16_BYTE( "a2klink4.bin", 0x00001, 0x80000, CRC(9c90cada) SHA1(10afbc7900ad876fddbe912c95bac1f575e0948e) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "a2klink5.bin", 0x00000, 0x80000, CRC(7083106a) SHA1(39e7da2ef91dda40b2a9d9b8d50c587d637fda54) )
	ROM_LOAD16_BYTE( "a2klink6.bin", 0x00001, 0x80000, CRC(ba0de415) SHA1(fb4d718b9ad95eaa0ca259605fc1f2916bd1b7b7) )
ROM_END

ROM_START( act2000v3 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "a2v33xt.bin", 0x00000, 0x40000, CRC(0e4fed4e) SHA1(d10ada62701f0165eac106d8b661d3c6a9597a71) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "a2klink2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "a2klink3.bin", 0x00000, 0x80000, CRC(10298268) SHA1(05b4c6ae90f069b67e7c17b7a74dc786888274a6) )
	ROM_LOAD16_BYTE( "a2klink4.bin", 0x00001, 0x80000, CRC(9c90cada) SHA1(10afbc7900ad876fddbe912c95bac1f575e0948e) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "a2klink5.bin", 0x00000, 0x80000, CRC(7083106a) SHA1(39e7da2ef91dda40b2a9d9b8d50c587d637fda54) )
	ROM_LOAD16_BYTE( "a2klink6.bin", 0x00001, 0x80000, CRC(ba0de415) SHA1(fb4d718b9ad95eaa0ca259605fc1f2916bd1b7b7) )
ROM_END

ROM_START( act2000d )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "a2d35r.bin", 0x00000, 0x40000, CRC(6a6af0c9) SHA1(9a644dacb658a226a69dac448c7b53ceccf6005b) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "a2klink2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "a2klink3.bin", 0x00000, 0x80000, CRC(10298268) SHA1(05b4c6ae90f069b67e7c17b7a74dc786888274a6) )
	ROM_LOAD16_BYTE( "a2klink4.bin", 0x00001, 0x80000, CRC(9c90cada) SHA1(10afbc7900ad876fddbe912c95bac1f575e0948e) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "a2klink5.bin", 0x00000, 0x80000, CRC(7083106a) SHA1(39e7da2ef91dda40b2a9d9b8d50c587d637fda54) )
	ROM_LOAD16_BYTE( "a2klink6.bin", 0x00001, 0x80000, CRC(ba0de415) SHA1(fb4d718b9ad95eaa0ca259605fc1f2916bd1b7b7) )
ROM_END

ROM_START( act2000d2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "a2d33xt.bin", 0x00000, 0x40000, CRC(743ae2b5) SHA1(e1a9ade074159756daacad827791dae971e99d9d) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "a2klink2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "a2klink3.bin", 0x00000, 0x80000, CRC(10298268) SHA1(05b4c6ae90f069b67e7c17b7a74dc786888274a6) )
	ROM_LOAD16_BYTE( "a2klink4.bin", 0x00001, 0x80000, CRC(9c90cada) SHA1(10afbc7900ad876fddbe912c95bac1f575e0948e) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "a2klink5.bin", 0x00000, 0x80000, CRC(7083106a) SHA1(39e7da2ef91dda40b2a9d9b8d50c587d637fda54) )
	ROM_LOAD16_BYTE( "a2klink6.bin", 0x00001, 0x80000, CRC(ba0de415) SHA1(fb4d718b9ad95eaa0ca259605fc1f2916bd1b7b7) )
ROM_END

ROM_START( act2000b )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "a2b35r.bin", 0x00000, 0x40000, CRC(b8a560a5) SHA1(0b819ddcef8f8026664987de85f7b1931f344354))

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "a2klink2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "a2klink3.bin", 0x00000, 0x80000, CRC(10298268) SHA1(05b4c6ae90f069b67e7c17b7a74dc786888274a6) )
	ROM_LOAD16_BYTE( "a2klink4.bin", 0x00001, 0x80000, CRC(9c90cada) SHA1(10afbc7900ad876fddbe912c95bac1f575e0948e) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "a2klink5.bin", 0x00000, 0x80000, CRC(7083106a) SHA1(39e7da2ef91dda40b2a9d9b8d50c587d637fda54) )
	ROM_LOAD16_BYTE( "a2klink6.bin", 0x00001, 0x80000, CRC(ba0de415) SHA1(fb4d718b9ad95eaa0ca259605fc1f2916bd1b7b7) )
ROM_END

ROM_START( act2000b2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "a2b33xt.bin", 0x00000, 0x40000, CRC(5a9375a8) SHA1(cc663d20e98fe143f4bf5f4cd15d35ff181bff5e) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "a2klink2.bin", 0x00000, 0x40000, CRC(3b0f5374) SHA1(7e7b185b62d1a321e2853b4b08e8ee2aa54933f5) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "a2klink3.bin", 0x00000, 0x80000, CRC(10298268) SHA1(05b4c6ae90f069b67e7c17b7a74dc786888274a6) )
	ROM_LOAD16_BYTE( "a2klink4.bin", 0x00001, 0x80000, CRC(9c90cada) SHA1(10afbc7900ad876fddbe912c95bac1f575e0948e) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "a2klink5.bin", 0x00000, 0x80000, CRC(7083106a) SHA1(39e7da2ef91dda40b2a9d9b8d50c587d637fda54) )
	ROM_LOAD16_BYTE( "a2klink6.bin", 0x00001, 0x80000, CRC(ba0de415) SHA1(fb4d718b9ad95eaa0ca259605fc1f2916bd1b7b7) )
ROM_END

ROM_START( pir2001 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pirat23n.bin", 0x00000, 0x40000, CRC(e11722bb) SHA1(cc4b729f4d7d72ffee15e7958335843027378ece) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "piratrom2.bin", 0x00000, 0x20000, CRC(eeb92009) SHA1(e6c69437a7fd0f9fae375bf0b6dcfd6226823cf2) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "piratrom3.bin", 0x00000, 0x80000, CRC(5a718b09) SHA1(7ea20a5c9cf8875b9c3cc95a708911fb87abebf7) )
	ROM_LOAD16_BYTE( "piratrom4.bin", 0x00001, 0x80000, CRC(123cdc93) SHA1(c4963c0a31eb25f15ec1902c9777643cf2c3e8c3) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "piratrom5.bin", 0x00000, 0x80000, CRC(6d7e502a) SHA1(4910a0bb1e779e04e87eb6cba092f976f85c0f96) )
	ROM_LOAD16_BYTE( "piratrom6.bin", 0x00001, 0x80000, CRC(470ff052) SHA1(b63293e2f244d992e64df085d6565931b982dcd3) )
ROM_END

ROM_START( pir2001a )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pirat23.bin", 0x00000, 0x40000, CRC(25ac8d18) SHA1(efc77735a418d298b16cba82ce1a0375dca2a7ef) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "piratrom2.bin", 0x00000, 0x20000, CRC(eeb92009) SHA1(e6c69437a7fd0f9fae375bf0b6dcfd6226823cf2) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "piratrom3.bin", 0x00000, 0x80000, CRC(5a718b09) SHA1(7ea20a5c9cf8875b9c3cc95a708911fb87abebf7) )
	ROM_LOAD16_BYTE( "piratrom4.bin", 0x00001, 0x80000, CRC(123cdc93) SHA1(c4963c0a31eb25f15ec1902c9777643cf2c3e8c3) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "piratrom5.bin", 0x00000, 0x80000, CRC(6d7e502a) SHA1(4910a0bb1e779e04e87eb6cba092f976f85c0f96) )
	ROM_LOAD16_BYTE( "piratrom6.bin", 0x00001, 0x80000, CRC(470ff052) SHA1(b63293e2f244d992e64df085d6565931b982dcd3) )
ROM_END

ROM_START( pir2001b )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pira22xt.bin", 0x00000, 0x40000, CRC(0412c601) SHA1(979d0bf26f8b2e6204e7d1cfdaeb89dc8e82cfce) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "piratrom2.bin", 0x00000, 0x20000, CRC(eeb92009) SHA1(e6c69437a7fd0f9fae375bf0b6dcfd6226823cf2) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "piratrom3.bin", 0x00000, 0x80000, CRC(5a718b09) SHA1(7ea20a5c9cf8875b9c3cc95a708911fb87abebf7) )
	ROM_LOAD16_BYTE( "piratrom4.bin", 0x00001, 0x80000, CRC(123cdc93) SHA1(c4963c0a31eb25f15ec1902c9777643cf2c3e8c3) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "piratrom5.bin", 0x00000, 0x80000, CRC(6d7e502a) SHA1(4910a0bb1e779e04e87eb6cba092f976f85c0f96) )
	ROM_LOAD16_BYTE( "piratrom6.bin", 0x00001, 0x80000, CRC(470ff052) SHA1(b63293e2f244d992e64df085d6565931b982dcd3) )
ROM_END

ROM_START( pir2001b2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pi1b24xt.bin", 0x00000, 0x40000, CRC(62adfe66) SHA1(e85ea2c0d00f29238f17c87e65a6b749336ffd50) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "piratrom2.bin", 0x00000, 0x20000, CRC(eeb92009) SHA1(e6c69437a7fd0f9fae375bf0b6dcfd6226823cf2) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "piratrom3.bin", 0x00000, 0x80000, CRC(5a718b09) SHA1(7ea20a5c9cf8875b9c3cc95a708911fb87abebf7) )
	ROM_LOAD16_BYTE( "piratrom4.bin", 0x00001, 0x80000, CRC(123cdc93) SHA1(c4963c0a31eb25f15ec1902c9777643cf2c3e8c3) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "piratrom5.bin", 0x00000, 0x80000, CRC(6d7e502a) SHA1(4910a0bb1e779e04e87eb6cba092f976f85c0f96) )
	ROM_LOAD16_BYTE( "piratrom6.bin", 0x00001, 0x80000, CRC(470ff052) SHA1(b63293e2f244d992e64df085d6565931b982dcd3) )
ROM_END

ROM_START( pir2001b3 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pi1b25r.bin", 0x00000, 0x40000, CRC(6f2624e4) SHA1(e1669d81bf708c65778d81ed4f5c793725edde3f) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "piratrom2.bin", 0x00000, 0x20000, CRC(eeb92009) SHA1(e6c69437a7fd0f9fae375bf0b6dcfd6226823cf2) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "piratrom3.bin", 0x00000, 0x80000, CRC(5a718b09) SHA1(7ea20a5c9cf8875b9c3cc95a708911fb87abebf7) )
	ROM_LOAD16_BYTE( "piratrom4.bin", 0x00001, 0x80000, CRC(123cdc93) SHA1(c4963c0a31eb25f15ec1902c9777643cf2c3e8c3) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "piratrom5.bin", 0x00000, 0x80000, CRC(6d7e502a) SHA1(4910a0bb1e779e04e87eb6cba092f976f85c0f96) )
	ROM_LOAD16_BYTE( "piratrom6.bin", 0x00001, 0x80000, CRC(470ff052) SHA1(b63293e2f244d992e64df085d6565931b982dcd3) )
ROM_END

ROM_START( pir2001v )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pi1v25r.bin", 0x00000, 0x40000, CRC(666207ea) SHA1(0d1fbd10aa85d4e5b8072266ce52b535b275fc5a) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "piratrom2.bin", 0x00000, 0x20000, CRC(eeb92009) SHA1(e6c69437a7fd0f9fae375bf0b6dcfd6226823cf2) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "piratrom3.bin", 0x00000, 0x80000, CRC(5a718b09) SHA1(7ea20a5c9cf8875b9c3cc95a708911fb87abebf7) )
	ROM_LOAD16_BYTE( "piratrom4.bin", 0x00001, 0x80000, CRC(123cdc93) SHA1(c4963c0a31eb25f15ec1902c9777643cf2c3e8c3) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "piratrom5.bin", 0x00000, 0x80000, CRC(6d7e502a) SHA1(4910a0bb1e779e04e87eb6cba092f976f85c0f96) )
	ROM_LOAD16_BYTE( "piratrom6.bin", 0x00001, 0x80000, CRC(470ff052) SHA1(b63293e2f244d992e64df085d6565931b982dcd3) )
ROM_END

ROM_START( pir2001v2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pi1v25e.bin", 0x00000, 0x40000, CRC(0440d844) SHA1(14f62aee8cb56cdfa399b8052181f60fcbcedbba) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "piratrom2.bin", 0x00000, 0x20000, CRC(eeb92009) SHA1(e6c69437a7fd0f9fae375bf0b6dcfd6226823cf2) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "piratrom3.bin", 0x00000, 0x80000, CRC(5a718b09) SHA1(7ea20a5c9cf8875b9c3cc95a708911fb87abebf7) )
	ROM_LOAD16_BYTE( "piratrom4.bin", 0x00001, 0x80000, CRC(123cdc93) SHA1(c4963c0a31eb25f15ec1902c9777643cf2c3e8c3) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "piratrom5.bin", 0x00000, 0x80000, CRC(6d7e502a) SHA1(4910a0bb1e779e04e87eb6cba092f976f85c0f96) )
	ROM_LOAD16_BYTE( "piratrom6.bin", 0x00001, 0x80000, CRC(470ff052) SHA1(b63293e2f244d992e64df085d6565931b982dcd3) )
ROM_END

ROM_START( pir2001v3 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pi1v24xt.bin", 0x00000, 0x40000, CRC(bc69b7e2) SHA1(bb4fc3ce17a9e97823bd9801fa549e5ddba6787d) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "piratrom2.bin", 0x00000, 0x20000, CRC(eeb92009) SHA1(e6c69437a7fd0f9fae375bf0b6dcfd6226823cf2) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "piratrom3.bin", 0x00000, 0x80000, CRC(5a718b09) SHA1(7ea20a5c9cf8875b9c3cc95a708911fb87abebf7) )
	ROM_LOAD16_BYTE( "piratrom4.bin", 0x00001, 0x80000, CRC(123cdc93) SHA1(c4963c0a31eb25f15ec1902c9777643cf2c3e8c3) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "piratrom5.bin", 0x00000, 0x80000, CRC(6d7e502a) SHA1(4910a0bb1e779e04e87eb6cba092f976f85c0f96) )
	ROM_LOAD16_BYTE( "piratrom6.bin", 0x00001, 0x80000, CRC(470ff052) SHA1(b63293e2f244d992e64df085d6565931b982dcd3) )
ROM_END

ROM_START( pir2001d )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pi1d25r.bin", 0x00000, 0x40000, CRC(579a753e) SHA1(82d70362c22d4a4f4836f1e10effdc05041bd425) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "piratrom2.bin", 0x00000, 0x20000, CRC(eeb92009) SHA1(e6c69437a7fd0f9fae375bf0b6dcfd6226823cf2) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "piratrom3.bin", 0x00000, 0x80000, CRC(5a718b09) SHA1(7ea20a5c9cf8875b9c3cc95a708911fb87abebf7) )
	ROM_LOAD16_BYTE( "piratrom4.bin", 0x00001, 0x80000, CRC(123cdc93) SHA1(c4963c0a31eb25f15ec1902c9777643cf2c3e8c3) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "piratrom5.bin", 0x00000, 0x80000, CRC(6d7e502a) SHA1(4910a0bb1e779e04e87eb6cba092f976f85c0f96) )
	ROM_LOAD16_BYTE( "piratrom6.bin", 0x00001, 0x80000, CRC(470ff052) SHA1(b63293e2f244d992e64df085d6565931b982dcd3) )
ROM_END

ROM_START( pir2001d2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pi1d24xt.bin", 0x00000, 0x40000,CRC(0e3e68ed) SHA1(a0e007a1f905dd6e7ba6a8202c9e21893ff819e3) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "piratrom2.bin", 0x00000, 0x20000, CRC(eeb92009) SHA1(e6c69437a7fd0f9fae375bf0b6dcfd6226823cf2) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "piratrom3.bin", 0x00000, 0x80000, CRC(5a718b09) SHA1(7ea20a5c9cf8875b9c3cc95a708911fb87abebf7) )
	ROM_LOAD16_BYTE( "piratrom4.bin", 0x00001, 0x80000, CRC(123cdc93) SHA1(c4963c0a31eb25f15ec1902c9777643cf2c3e8c3) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "piratrom5.bin", 0x00000, 0x80000, CRC(6d7e502a) SHA1(4910a0bb1e779e04e87eb6cba092f976f85c0f96) )
	ROM_LOAD16_BYTE( "piratrom6.bin", 0x00001, 0x80000, CRC(470ff052) SHA1(b63293e2f244d992e64df085d6565931b982dcd3) )
ROM_END

ROM_START( pir2002 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pi218n.bin", 0x00000, 0x40000,  CRC(bd6a35f5) SHA1(1cf5c7e65f3d99aee3579d890dbac3c818735307) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "pirom2.bin", 0x00000, 0x20000, CRC(eeb92009) SHA1(e6c69437a7fd0f9fae375bf0b6dcfd6226823cf2) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "pirom3.bin", 0x00000, 0x80000, CRC(ad175fea) SHA1(07585fcb0d4828fb2b99bebfe583e54a835636ed) )
	ROM_LOAD16_BYTE( "pirom4.bin", 0x00001, 0x80000, CRC(a94061ec) SHA1(2c3b37a1144a873f0b4b884cbeb938947270f5a3) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "pirom5.bin", 0x00000, 0x80000, CRC(c3ccb77b) SHA1(39ab58e2e55a7fabed0a0c8e5777b9be10ae67ae) )
	ROM_LOAD16_BYTE( "pirom6.bin", 0x00001, 0x80000, CRC(c64bc2e5) SHA1(cfb231aa47d6e57481c24a4ba9d8623ed0fca58e) )
ROM_END

ROM_START( pir2002a )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pi218.bin", 0x00000, 0x40000, CRC(1480722d) SHA1(bd46fa6011caebc63ebd8cd2765c5b61ce379b85) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "pirom2.bin", 0x00000, 0x20000, CRC(eeb92009) SHA1(e6c69437a7fd0f9fae375bf0b6dcfd6226823cf2) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "pirom3.bin", 0x00000, 0x80000, CRC(ad175fea) SHA1(07585fcb0d4828fb2b99bebfe583e54a835636ed) )
	ROM_LOAD16_BYTE( "pirom4.bin", 0x00001, 0x80000, CRC(a94061ec) SHA1(2c3b37a1144a873f0b4b884cbeb938947270f5a3) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "pirom5.bin", 0x00000, 0x80000, CRC(c3ccb77b) SHA1(39ab58e2e55a7fabed0a0c8e5777b9be10ae67ae) )
	ROM_LOAD16_BYTE( "pirom6.bin", 0x00001, 0x80000, CRC(c64bc2e5) SHA1(cfb231aa47d6e57481c24a4ba9d8623ed0fca58e) )
ROM_END

ROM_START( pir2002b )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pi217xt.bin", 0x00000, 0x40000, CRC(0cc369cd) SHA1(7255fe1f544df248f41e6586d2632d65de0a5a98) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "pirom2.bin", 0x00000, 0x20000, CRC(eeb92009) SHA1(e6c69437a7fd0f9fae375bf0b6dcfd6226823cf2) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "pirom3.bin", 0x00000, 0x80000, CRC(ad175fea) SHA1(07585fcb0d4828fb2b99bebfe583e54a835636ed) )
	ROM_LOAD16_BYTE( "pirom4.bin", 0x00001, 0x80000, CRC(a94061ec) SHA1(2c3b37a1144a873f0b4b884cbeb938947270f5a3) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "pirom5.bin", 0x00000, 0x80000, CRC(c3ccb77b) SHA1(39ab58e2e55a7fabed0a0c8e5777b9be10ae67ae) )
	ROM_LOAD16_BYTE( "pirom6.bin", 0x00001, 0x80000, CRC(c64bc2e5) SHA1(cfb231aa47d6e57481c24a4ba9d8623ed0fca58e) )
ROM_END

/* these had the pir2001 sound rom in, mistake? */
ROM_START( pir2002b2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pi2b19xt.bin", 0x00000, 0x40000, CRC(c9eed644) SHA1(6cd40196bdd8e84738c970198e770f87964aab5d) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "pirom2.bin", 0x00000, 0x20000, CRC(eeb92009) SHA1(e6c69437a7fd0f9fae375bf0b6dcfd6226823cf2) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "pirom3.bin", 0x00000, 0x80000, CRC(ad175fea) SHA1(07585fcb0d4828fb2b99bebfe583e54a835636ed) )
	ROM_LOAD16_BYTE( "pirom4.bin", 0x00001, 0x80000, CRC(a94061ec) SHA1(2c3b37a1144a873f0b4b884cbeb938947270f5a3) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "pirom5.bin", 0x00000, 0x80000, CRC(c3ccb77b) SHA1(39ab58e2e55a7fabed0a0c8e5777b9be10ae67ae) )
	ROM_LOAD16_BYTE( "pirom6.bin", 0x00001, 0x80000, CRC(c64bc2e5) SHA1(cfb231aa47d6e57481c24a4ba9d8623ed0fca58e) )
ROM_END

ROM_START( pir2002b3 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pi2b20r.bin", 0x00000, 0x40000,  CRC(4b2e45c0) SHA1(b96ba54034a0e61d53e317559bfe83f337e63618) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "pirom2.bin", 0x00000, 0x20000, CRC(eeb92009) SHA1(e6c69437a7fd0f9fae375bf0b6dcfd6226823cf2) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "pirom3.bin", 0x00000, 0x80000, CRC(ad175fea) SHA1(07585fcb0d4828fb2b99bebfe583e54a835636ed) )
	ROM_LOAD16_BYTE( "pirom4.bin", 0x00001, 0x80000, CRC(a94061ec) SHA1(2c3b37a1144a873f0b4b884cbeb938947270f5a3) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "pirom5.bin", 0x00000, 0x80000, CRC(c3ccb77b) SHA1(39ab58e2e55a7fabed0a0c8e5777b9be10ae67ae) )
	ROM_LOAD16_BYTE( "pirom6.bin", 0x00001, 0x80000, CRC(c64bc2e5) SHA1(cfb231aa47d6e57481c24a4ba9d8623ed0fca58e) )
ROM_END

ROM_START( pir2002v )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pi2v20r.bin", 0x00000, 0x40000,  CRC(e4155252) SHA1(136ac929633bc6ee759285dcdb725aaaf7cdf225) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "pirom2.bin", 0x00000, 0x20000, CRC(eeb92009) SHA1(e6c69437a7fd0f9fae375bf0b6dcfd6226823cf2) )



	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "pirom3.bin", 0x00000, 0x80000, CRC(ad175fea) SHA1(07585fcb0d4828fb2b99bebfe583e54a835636ed) )
	ROM_LOAD16_BYTE( "pirom4.bin", 0x00001, 0x80000, CRC(a94061ec) SHA1(2c3b37a1144a873f0b4b884cbeb938947270f5a3) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "pirom5.bin", 0x00000, 0x80000, CRC(c3ccb77b) SHA1(39ab58e2e55a7fabed0a0c8e5777b9be10ae67ae) )
	ROM_LOAD16_BYTE( "pirom6.bin", 0x00001, 0x80000, CRC(c64bc2e5) SHA1(cfb231aa47d6e57481c24a4ba9d8623ed0fca58e) )
ROM_END

ROM_START( pir2002v2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pi2v20e.bin", 0x00000, 0x40000,  CRC(208fec36) SHA1(779f87cb436e7d59b6c410921b030430020577ec) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "pirom2.bin", 0x00000, 0x20000, CRC(eeb92009) SHA1(e6c69437a7fd0f9fae375bf0b6dcfd6226823cf2) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "pirom3.bin", 0x00000, 0x80000, CRC(ad175fea) SHA1(07585fcb0d4828fb2b99bebfe583e54a835636ed) )
	ROM_LOAD16_BYTE( "pirom4.bin", 0x00001, 0x80000, CRC(a94061ec) SHA1(2c3b37a1144a873f0b4b884cbeb938947270f5a3) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "pirom5.bin", 0x00000, 0x80000, CRC(c3ccb77b) SHA1(39ab58e2e55a7fabed0a0c8e5777b9be10ae67ae) )
	ROM_LOAD16_BYTE( "pirom6.bin", 0x00001, 0x80000, CRC(c64bc2e5) SHA1(cfb231aa47d6e57481c24a4ba9d8623ed0fca58e) )
ROM_END

ROM_START( pir2002v3 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pi2v19xt.bin", 0x00000, 0x40000, CRC(0ef73818) SHA1(7d1c856c78f4d7b36f318725de3dffb5ad9279fe) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "pirom2.bin", 0x00000, 0x20000, CRC(eeb92009) SHA1(e6c69437a7fd0f9fae375bf0b6dcfd6226823cf2) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "pirom3.bin", 0x00000, 0x80000, CRC(ad175fea) SHA1(07585fcb0d4828fb2b99bebfe583e54a835636ed) )
	ROM_LOAD16_BYTE( "pirom4.bin", 0x00001, 0x80000, CRC(a94061ec) SHA1(2c3b37a1144a873f0b4b884cbeb938947270f5a3) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "pirom5.bin", 0x00000, 0x80000, CRC(c3ccb77b) SHA1(39ab58e2e55a7fabed0a0c8e5777b9be10ae67ae) )
	ROM_LOAD16_BYTE( "pirom6.bin", 0x00001, 0x80000, CRC(c64bc2e5) SHA1(cfb231aa47d6e57481c24a4ba9d8623ed0fca58e) )
ROM_END

ROM_START( pir2002d )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pi2d20r.bin", 0x00000, 0x40000,  CRC(83a264c4) SHA1(7de1902f5b63d6c44df5726c450ff21b5d911ec4) )


	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "pirom2.bin", 0x00000, 0x20000, CRC(eeb92009) SHA1(e6c69437a7fd0f9fae375bf0b6dcfd6226823cf2) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "pirom3.bin", 0x00000, 0x80000, CRC(ad175fea) SHA1(07585fcb0d4828fb2b99bebfe583e54a835636ed) )
	ROM_LOAD16_BYTE( "pirom4.bin", 0x00001, 0x80000, CRC(a94061ec) SHA1(2c3b37a1144a873f0b4b884cbeb938947270f5a3) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "pirom5.bin", 0x00000, 0x80000, CRC(c3ccb77b) SHA1(39ab58e2e55a7fabed0a0c8e5777b9be10ae67ae) )
	ROM_LOAD16_BYTE( "pirom6.bin", 0x00001, 0x80000, CRC(c64bc2e5) SHA1(cfb231aa47d6e57481c24a4ba9d8623ed0fca58e) )
ROM_END

ROM_START( pir2002d2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "pi2d19xt.bin", 0x00000, 0x40000, CRC(1c045c9a) SHA1(dd3c6d2c1f084b4af262e52339d0c25c7e733b70) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "pirom2.bin", 0x00000, 0x20000, CRC(eeb92009) SHA1(e6c69437a7fd0f9fae375bf0b6dcfd6226823cf2) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "pirom3.bin", 0x00000, 0x80000, CRC(ad175fea) SHA1(07585fcb0d4828fb2b99bebfe583e54a835636ed) )
	ROM_LOAD16_BYTE( "pirom4.bin", 0x00001, 0x80000, CRC(a94061ec) SHA1(2c3b37a1144a873f0b4b884cbeb938947270f5a3) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "pirom5.bin", 0x00000, 0x80000, CRC(c3ccb77b) SHA1(39ab58e2e55a7fabed0a0c8e5777b9be10ae67ae) )
	ROM_LOAD16_BYTE( "pirom6.bin", 0x00001, 0x80000, CRC(c64bc2e5) SHA1(cfb231aa47d6e57481c24a4ba9d8623ed0fca58e) )
ROM_END

ROM_START( classice )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fcs16lt.bin", 0x00000, 0x40000, CRC(e4b3437a) SHA1(2ecbaead72bb20af58c7f470097901ac1c58f296) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fcsrom2.bin", 0x00000, 0x3ffff, BAD_DUMP CRC(4a96ab78) SHA1(b8f98cd9789ba5cc13eacf34db765ca8d5635903) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fcsrom3.bin", 0x00000, 0x40000, CRC(0ba6207a) SHA1(06d1b5826da3af2cb55c64ee69772b7fb3e6bf89) )
	ROM_LOAD16_BYTE( "fcsrom4.bin", 0x00001, 0x40000, CRC(149dcf7d) SHA1(1f4e0f54cdb22ee9a867861a2a7d659cd339a0a2) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fcsrom5.bin", 0x00000, 0x40000, CRC(fb827363) SHA1(ff9630e8be8facbff040f8a23bf5ff66c62609df) )
	ROM_LOAD16_BYTE( "fcsrom6.bin", 0x00001, 0x40000, CRC(9ec17dcd) SHA1(c1aefb7711feac1e9642eecbd41a1782d30bf7fa) )
ROM_END

ROM_START( classicea )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fcs16r.bin", 0x00000, 0x40000, CRC(0813e904) SHA1(87b6bb3c1ac17eb663673c948e6c33d1058c22e2) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fcsrom2.bin", 0x00000, 0x3ffff, BAD_DUMP CRC(4a96ab78) SHA1(b8f98cd9789ba5cc13eacf34db765ca8d5635903) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fcsrom3.bin", 0x00000, 0x40000, CRC(0ba6207a) SHA1(06d1b5826da3af2cb55c64ee69772b7fb3e6bf89) )
	ROM_LOAD16_BYTE( "fcsrom4.bin", 0x00001, 0x40000, CRC(149dcf7d) SHA1(1f4e0f54cdb22ee9a867861a2a7d659cd339a0a2) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fcsrom5.bin", 0x00000, 0x40000, CRC(fb827363) SHA1(ff9630e8be8facbff040f8a23bf5ff66c62609df) )
	ROM_LOAD16_BYTE( "fcsrom6.bin", 0x00001, 0x40000, CRC(9ec17dcd) SHA1(c1aefb7711feac1e9642eecbd41a1782d30bf7fa) )
ROM_END

ROM_START( classiced )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fcsd16r.bin", 0x00000, 0x40000, CRC(097dd178) SHA1(b5e251ce8fb323d20ff3722d048d98c4fab0f4a4) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fcsrom2.bin", 0x00000, 0x3ffff, BAD_DUMP CRC(4a96ab78) SHA1(b8f98cd9789ba5cc13eacf34db765ca8d5635903) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fcsrom3.bin", 0x00000, 0x40000, CRC(0ba6207a) SHA1(06d1b5826da3af2cb55c64ee69772b7fb3e6bf89) )
	ROM_LOAD16_BYTE( "fcsrom4.bin", 0x00001, 0x40000, CRC(149dcf7d) SHA1(1f4e0f54cdb22ee9a867861a2a7d659cd339a0a2) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fcsrom5.bin", 0x00000, 0x40000, CRC(fb827363) SHA1(ff9630e8be8facbff040f8a23bf5ff66c62609df) )
	ROM_LOAD16_BYTE( "fcsrom6.bin", 0x00001, 0x40000, CRC(9ec17dcd) SHA1(c1aefb7711feac1e9642eecbd41a1782d30bf7fa) )
ROM_END

ROM_START( classiced2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fcsd16lt.bin", 0x00000, 0x40000, CRC(623c5e2e) SHA1(63bbb7b1f8668828c5c8da8ae025077eca0b5d53) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fcsrom2.bin", 0x00000, 0x3ffff, BAD_DUMP CRC(4a96ab78) SHA1(b8f98cd9789ba5cc13eacf34db765ca8d5635903) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fcsrom3.bin", 0x00000, 0x40000, CRC(0ba6207a) SHA1(06d1b5826da3af2cb55c64ee69772b7fb3e6bf89) )
	ROM_LOAD16_BYTE( "fcsrom4.bin", 0x00001, 0x40000, CRC(149dcf7d) SHA1(1f4e0f54cdb22ee9a867861a2a7d659cd339a0a2) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fcsrom5.bin", 0x00000, 0x40000, CRC(fb827363) SHA1(ff9630e8be8facbff040f8a23bf5ff66c62609df) )
	ROM_LOAD16_BYTE( "fcsrom6.bin", 0x00001, 0x40000, CRC(9ec17dcd) SHA1(c1aefb7711feac1e9642eecbd41a1782d30bf7fa) )
ROM_END

ROM_START( classiced3 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fcsd16e.bin", 0x00000, 0x40000, CRC(74134183) SHA1(b59727dc0fae022e97bb60c444a3a78d811aa1ad) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fcsrom2.bin", 0x00000, 0x3ffff, BAD_DUMP CRC(4a96ab78) SHA1(b8f98cd9789ba5cc13eacf34db765ca8d5635903) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fcsrom3.bin", 0x00000, 0x40000, CRC(0ba6207a) SHA1(06d1b5826da3af2cb55c64ee69772b7fb3e6bf89) )
	ROM_LOAD16_BYTE( "fcsrom4.bin", 0x00001, 0x40000, CRC(149dcf7d) SHA1(1f4e0f54cdb22ee9a867861a2a7d659cd339a0a2) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fcsrom5.bin", 0x00000, 0x40000, CRC(fb827363) SHA1(ff9630e8be8facbff040f8a23bf5ff66c62609df) )
	ROM_LOAD16_BYTE( "fcsrom6.bin", 0x00001, 0x40000, CRC(9ec17dcd) SHA1(c1aefb7711feac1e9642eecbd41a1782d30bf7fa) )
ROM_END

ROM_START( classicev )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fcsv16r.bin", 0x00000, 0x40000, CRC(e0744057) SHA1(bb389cce5d77eed6f74eb46afa90712f803f357b) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fcsrom2.bin", 0x00000, 0x3ffff, BAD_DUMP CRC(4a96ab78) SHA1(b8f98cd9789ba5cc13eacf34db765ca8d5635903) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fcsrom3.bin", 0x00000, 0x40000, CRC(0ba6207a) SHA1(06d1b5826da3af2cb55c64ee69772b7fb3e6bf89) )
	ROM_LOAD16_BYTE( "fcsrom4.bin", 0x00001, 0x40000, CRC(149dcf7d) SHA1(1f4e0f54cdb22ee9a867861a2a7d659cd339a0a2) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fcsrom5.bin", 0x00000, 0x40000, CRC(fb827363) SHA1(ff9630e8be8facbff040f8a23bf5ff66c62609df) )
	ROM_LOAD16_BYTE( "fcsrom6.bin", 0x00001, 0x40000, CRC(9ec17dcd) SHA1(c1aefb7711feac1e9642eecbd41a1782d30bf7fa) )
ROM_END

ROM_START( classicev2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fcsv16lt.bin", 0x00000, 0x40000, CRC(33393a1f) SHA1(03da07380129f07e5126b5faa37157b97f2c902e) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fcsrom2.bin", 0x00000, 0x3ffff, BAD_DUMP CRC(4a96ab78) SHA1(b8f98cd9789ba5cc13eacf34db765ca8d5635903) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fcsrom3.bin", 0x00000, 0x40000, CRC(0ba6207a) SHA1(06d1b5826da3af2cb55c64ee69772b7fb3e6bf89) )
	ROM_LOAD16_BYTE( "fcsrom4.bin", 0x00001, 0x40000, CRC(149dcf7d) SHA1(1f4e0f54cdb22ee9a867861a2a7d659cd339a0a2) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fcsrom5.bin", 0x00000, 0x40000, CRC(fb827363) SHA1(ff9630e8be8facbff040f8a23bf5ff66c62609df) )
	ROM_LOAD16_BYTE( "fcsrom6.bin", 0x00001, 0x40000, CRC(9ec17dcd) SHA1(c1aefb7711feac1e9642eecbd41a1782d30bf7fa) )
ROM_END

ROM_START( classicev3 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fcsv16e.bin", 0x00000, 0x40000, CRC(fe472583) SHA1(dd8642c33456d62b47e272fb63d4bf88e11d4c70) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fcsrom2.bin", 0x00000, 0x3ffff, BAD_DUMP CRC(4a96ab78) SHA1(b8f98cd9789ba5cc13eacf34db765ca8d5635903) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fcsrom3.bin", 0x00000, 0x40000, CRC(0ba6207a) SHA1(06d1b5826da3af2cb55c64ee69772b7fb3e6bf89) )
	ROM_LOAD16_BYTE( "fcsrom4.bin", 0x00001, 0x40000, CRC(149dcf7d) SHA1(1f4e0f54cdb22ee9a867861a2a7d659cd339a0a2) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fcsrom5.bin", 0x00000, 0x40000, CRC(fb827363) SHA1(ff9630e8be8facbff040f8a23bf5ff66c62609df) )
	ROM_LOAD16_BYTE( "fcsrom6.bin", 0x00001, 0x40000, CRC(9ec17dcd) SHA1(c1aefb7711feac1e9642eecbd41a1782d30bf7fa) )
ROM_END

ROM_START( seawld )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "swd16r.bin", 0x00000, 0x80000, CRC(081c84c1) SHA1(5f0d40c38ca26d3633cfe4c7ead2773a1dcc177d) )

	ROM_REGION( 0x010000, "debugram", ROMREGION_ERASE00 ) /* DEBUG */


	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "swrom2.bin", 0x00000, 0x40000, CRC(e1afe0ad) SHA1(097233255b486944b79a8504b4312173ab1aad06) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "swrom3.bin", 0x00000, 0x80000, CRC(091b6966) SHA1(4ac17ca80cdb584a4d32f81688ce374bd8bd9cc6) )
	ROM_LOAD16_BYTE( "swrom4.bin", 0x00001, 0x80000, CRC(539651dc) SHA1(45473cd7205ba0c0e44c76d3f6a8fa2f66b2798c) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "swrom5.bin", 0x00000, 0x80000, CRC(cd6aa69f) SHA1(abcbda547b0c6f4a03ed3500f55ff32bc23bedeb) )
	ROM_LOAD16_BYTE( "swrom6.bin", 0x00001, 0x80000, CRC(5c9a4847) SHA1(f19aca69f42282e3e88e50e2b4fe05cde990a3e6) )
ROM_END

ROM_START( seawlda )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "swv16e.bin", 0x00000, 0x80000, CRC(3f53a6b0) SHA1(2d00f3b5c04b47551f23799a3bcba29ab38ff63c) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "swrom2.bin", 0x00000, 0x40000, CRC(e1afe0ad) SHA1(097233255b486944b79a8504b4312173ab1aad06) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "swrom3.bin", 0x00000, 0x80000, CRC(091b6966) SHA1(4ac17ca80cdb584a4d32f81688ce374bd8bd9cc6) )
	ROM_LOAD16_BYTE( "swrom4.bin", 0x00001, 0x80000, CRC(539651dc) SHA1(45473cd7205ba0c0e44c76d3f6a8fa2f66b2798c) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "swrom5.bin", 0x00000, 0x80000, CRC(cd6aa69f) SHA1(abcbda547b0c6f4a03ed3500f55ff32bc23bedeb) )
	ROM_LOAD16_BYTE( "swrom6.bin", 0x00001, 0x80000, CRC(5c9a4847) SHA1(f19aca69f42282e3e88e50e2b4fe05cde990a3e6) )
ROM_END


ROM_START( moneymac )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "mmv17r.bin", 0x00000, 0x40000, CRC(2c92617c) SHA1(85332981acf1938bb42b6ef432a57331ef3530a1) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "mmrom2.bin", 0x00000, 0x40000, CRC(fc3195e6) SHA1(a13c22c0cd5cdbc833e0f7e229ce4afe2cf2b466) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "mmrom3.bin", 0x00000, 0x80000, CRC(1ef6ee35) SHA1(0617121b44fb0866fdc992aa35a8c2e5f696b69a) )
	ROM_LOAD16_BYTE( "mmrom4.bin", 0x00001, 0x80000, CRC(f9f979b5) SHA1(994bd28fc82a6e10126e5c2e7c1938f6a20a49a8) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "mmrom5.bin", 0x00000, 0x80000, CRC(7009308d) SHA1(8c09bfa025ae5cdab5c488af9cf1747da5d1ac67) )
	ROM_LOAD16_BYTE( "mmrom6.bin", 0x00001, 0x80000, CRC(828dde28) SHA1(3024d5d449acce1f78254053866f3aa8d36aff53) )
ROM_END

ROM_START( moneymacv )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "mmv17e.bin", 0x00000, 0x40000, CRC(53e43e39) SHA1(f5a02251825716cfa1f30afd6fd3b6c0de7e3146) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "mmrom2.bin", 0x00000, 0x40000, CRC(fc3195e6) SHA1(a13c22c0cd5cdbc833e0f7e229ce4afe2cf2b466) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "mmrom3.bin", 0x00000, 0x80000, CRC(1ef6ee35) SHA1(0617121b44fb0866fdc992aa35a8c2e5f696b69a) )
	ROM_LOAD16_BYTE( "mmrom4.bin", 0x00001, 0x80000, CRC(f9f979b5) SHA1(994bd28fc82a6e10126e5c2e7c1938f6a20a49a8) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "mmrom5.bin", 0x00000, 0x80000, CRC(7009308d) SHA1(8c09bfa025ae5cdab5c488af9cf1747da5d1ac67) )
	ROM_LOAD16_BYTE( "mmrom6.bin", 0x00001, 0x80000, CRC(828dde28) SHA1(3024d5d449acce1f78254053866f3aa8d36aff53) )
ROM_END

ROM_START( moneymacv2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "mmv17lt.bin", 0x00000, 0x40000, CRC(5f695601) SHA1(1fc099bea8d7c6ea76ec933193483fedd993823d) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "mmrom2.bin", 0x00000, 0x40000, CRC(fc3195e6) SHA1(a13c22c0cd5cdbc833e0f7e229ce4afe2cf2b466) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "mmrom3.bin", 0x00000, 0x80000, CRC(1ef6ee35) SHA1(0617121b44fb0866fdc992aa35a8c2e5f696b69a) )
	ROM_LOAD16_BYTE( "mmrom4.bin", 0x00001, 0x80000, CRC(f9f979b5) SHA1(994bd28fc82a6e10126e5c2e7c1938f6a20a49a8) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "mmrom5.bin", 0x00000, 0x80000, CRC(7009308d) SHA1(8c09bfa025ae5cdab5c488af9cf1747da5d1ac67) )
	ROM_LOAD16_BYTE( "mmrom6.bin", 0x00001, 0x80000, CRC(828dde28) SHA1(3024d5d449acce1f78254053866f3aa8d36aff53) )
ROM_END

ROM_START( moneymacd )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "mmd17r.bin", 0x00000, 0x40000, CRC(66dbacdd) SHA1(9d0440a3d8c58860cd2e59310677320b6e40c46b) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "mmrom2.bin", 0x00000, 0x40000, CRC(fc3195e6) SHA1(a13c22c0cd5cdbc833e0f7e229ce4afe2cf2b466) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "mmrom3.bin", 0x00000, 0x80000, CRC(1ef6ee35) SHA1(0617121b44fb0866fdc992aa35a8c2e5f696b69a) )
	ROM_LOAD16_BYTE( "mmrom4.bin", 0x00001, 0x80000, CRC(f9f979b5) SHA1(994bd28fc82a6e10126e5c2e7c1938f6a20a49a8) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "mmrom5.bin", 0x00000, 0x80000, CRC(7009308d) SHA1(8c09bfa025ae5cdab5c488af9cf1747da5d1ac67) )
	ROM_LOAD16_BYTE( "mmrom6.bin", 0x00001, 0x80000, CRC(828dde28) SHA1(3024d5d449acce1f78254053866f3aa8d36aff53) )
ROM_END

ROM_START( moneymacd2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "mmd17lt.bin", 0x00000, 0x40000, CRC(85a72381) SHA1(eaee2504a205b3b8ce7cbe1f69d276ad131b0554) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "mmrom2.bin", 0x00000, 0x40000, CRC(fc3195e6) SHA1(a13c22c0cd5cdbc833e0f7e229ce4afe2cf2b466) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "mmrom3.bin", 0x00000, 0x80000, CRC(1ef6ee35) SHA1(0617121b44fb0866fdc992aa35a8c2e5f696b69a) )
	ROM_LOAD16_BYTE( "mmrom4.bin", 0x00001, 0x80000, CRC(f9f979b5) SHA1(994bd28fc82a6e10126e5c2e7c1938f6a20a49a8) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "mmrom5.bin", 0x00000, 0x80000, CRC(7009308d) SHA1(8c09bfa025ae5cdab5c488af9cf1747da5d1ac67) )
	ROM_LOAD16_BYTE( "mmrom6.bin", 0x00001, 0x80000, CRC(828dde28) SHA1(3024d5d449acce1f78254053866f3aa8d36aff53) )
ROM_END


ROM_START( atworld )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "awd13r.bin", 0x00000, 0x80000, CRC(786079a8) SHA1(862abc511c5ac0d667c6b9abd914ce6035e9aed9) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "aw_rom2.bin", 0x00000, 0x40000, CRC(aff26a52) SHA1(176fb42d735a85cdc3b74d6dde76fea9115bf36d) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "aw_rom3.bin", 0x00000, 0x80000, CRC(36db794a) SHA1(a5cb32fc401faf52e221f0a4d8bbfae819e7d08b) )
	ROM_LOAD16_BYTE( "aw_rom4.bin", 0x00001, 0x80000, CRC(3927d187) SHA1(4d6e509ec6cc33e6985142894bbce547e1ee9f4f) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "aw_rom5.bin", 0x00000, 0x80000, CRC(c461c4d5) SHA1(2815511f8ae9b74c44aa9987eebf1a14642b4458) )
	ROM_LOAD16_BYTE( "aw_rom6.bin", 0x00001, 0x80000, CRC(686c9f2d) SHA1(94da22c775292020aa00c8f12f833a7f5c70ec36) )
ROM_END

ROM_START( atworlda )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "awd13e.bin", 0x00000, 0x80000, CRC(ec46b48d) SHA1(bfae55520bb36a6dfb55e12b115e818d9cd060e7) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "aw_rom2.bin", 0x00000, 0x40000, CRC(aff26a52) SHA1(176fb42d735a85cdc3b74d6dde76fea9115bf36d) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "aw_rom3.bin", 0x00000, 0x80000, CRC(36db794a) SHA1(a5cb32fc401faf52e221f0a4d8bbfae819e7d08b) )
	ROM_LOAD16_BYTE( "aw_rom4.bin", 0x00001, 0x80000, CRC(3927d187) SHA1(4d6e509ec6cc33e6985142894bbce547e1ee9f4f) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "aw_rom5.bin", 0x00000, 0x80000, CRC(c461c4d5) SHA1(2815511f8ae9b74c44aa9987eebf1a14642b4458) )
	ROM_LOAD16_BYTE( "aw_rom6.bin", 0x00001, 0x80000, CRC(686c9f2d) SHA1(94da22c775292020aa00c8f12f833a7f5c70ec36) )
ROM_END

#if 1 // incomplete / mixed sets etc. sort these first before doing anything else with them
ROM_START( fb5 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fb5b15h.bin", 0x00000, 0x40000, CRC(faba08b8) SHA1(4763f691b563ba23cc3edf86c18cdcda8c415003) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb5rom2.bin", 0x00000, 0x3ffff, BAD_DUMP CRC(bf49ba49) SHA1(eea40e34298f7fd98771f0869ef541c5e1514f2a) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb5rom3.bin", 0x00000, 0x80000, CRC(838c992d) SHA1(689aeab1916f0f92995ae417cb5ec84a216917a0) )
	ROM_LOAD16_BYTE( "fb5rom4.bin", 0x00001, 0x80000, CRC(17bbec6e) SHA1(e94479372d60f22e6598dfafc11a6a9b112e8699) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb5rom5.bin", 0x00000, 0x80000, CRC(f358df3e) SHA1(a82980ee04160e0ec541e114c198944b2802d0ff) )
	ROM_LOAD16_BYTE( "fb5rom6.bin", 0x00001, 0x80000, CRC(7fba66f3) SHA1(7889394940db76172e1e78be852b0362197cbd8b) )
ROM_END

ROM_START( fb5c )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fb5c15h.bin", 0x00000, 0x40000, CRC(62897a2b) SHA1(7a30c6453b9e04d25686deb97e25b89e49a6305d) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb5rom2.bin", 0x00000, 0x3ffff, BAD_DUMP CRC(bf49ba49) SHA1(eea40e34298f7fd98771f0869ef541c5e1514f2a) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb5rom3.bin", 0x00000, 0x80000, CRC(838c992d) SHA1(689aeab1916f0f92995ae417cb5ec84a216917a0) )
	ROM_LOAD16_BYTE( "fb5rom4.bin", 0x00001, 0x80000, CRC(17bbec6e) SHA1(e94479372d60f22e6598dfafc11a6a9b112e8699) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb5rom5.bin", 0x00000, 0x80000, CRC(f358df3e) SHA1(a82980ee04160e0ec541e114c198944b2802d0ff) )
	ROM_LOAD16_BYTE( "fb5rom6.bin", 0x00001, 0x80000, CRC(7fba66f3) SHA1(7889394940db76172e1e78be852b0362197cbd8b) )
ROM_END

ROM_START( fb5d )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fb5d15h.bin", 0x00000, 0x40000, CRC(231b4083) SHA1(a009cae4943ba8d6a56eb4d70bc8b50f98b62fde) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb5rom2.bin", 0x00000, 0x3ffff, BAD_DUMP CRC(bf49ba49) SHA1(eea40e34298f7fd98771f0869ef541c5e1514f2a) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb5rom3.bin", 0x00000, 0x80000, CRC(838c992d) SHA1(689aeab1916f0f92995ae417cb5ec84a216917a0) )
	ROM_LOAD16_BYTE( "fb5rom4.bin", 0x00001, 0x80000, CRC(17bbec6e) SHA1(e94479372d60f22e6598dfafc11a6a9b112e8699) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb5rom5.bin", 0x00000, 0x80000, CRC(f358df3e) SHA1(a82980ee04160e0ec541e114c198944b2802d0ff) )
	ROM_LOAD16_BYTE( "fb5rom6.bin", 0x00001, 0x80000, CRC(7fba66f3) SHA1(7889394940db76172e1e78be852b0362197cbd8b) )
ROM_END

ROM_START( fb5v )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "fb5v15h.bin", 0x00000, 0x40000, CRC(c6b117f5) SHA1(186dcfd9fd9b077036af54f8632ba70118f2f510) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb5rom2.bin", 0x00000, 0x3ffff, BAD_DUMP CRC(bf49ba49) SHA1(eea40e34298f7fd98771f0869ef541c5e1514f2a) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb5rom3.bin", 0x00000, 0x80000, CRC(838c992d) SHA1(689aeab1916f0f92995ae417cb5ec84a216917a0) )
	ROM_LOAD16_BYTE( "fb5rom4.bin", 0x00001, 0x80000, CRC(17bbec6e) SHA1(e94479372d60f22e6598dfafc11a6a9b112e8699) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb5rom5.bin", 0x00000, 0x80000, CRC(f358df3e) SHA1(a82980ee04160e0ec541e114c198944b2802d0ff) )
	ROM_LOAD16_BYTE( "fb5rom6.bin", 0x00001, 0x80000, CRC(7fba66f3) SHA1(7889394940db76172e1e78be852b0362197cbd8b) )
ROM_END


ROM_START( fb6 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "f06d17e.bin", 0x00000, 0x40000,  CRC(3c13d847) SHA1(c3ec365a507b960d8e97c19e1334da8fb3c9f4cf) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb06rom2.bin", 0x00000, 0x40000, CRC(f1adbcd5) SHA1(90a8830d000eb634c2db8a09431daba6cdcb2d34) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb06rom3.bin", 0x00000, 0x40000, CRC(63159a3a) SHA1(77e5801506ea58df73c406c8675dc8c06ba92313) )
	ROM_LOAD16_BYTE( "fb06rom4.bin", 0x00001, 0x40000, CRC(e4f4f04e) SHA1(412cfec7235455c09cffde5ca05c3e2fe4a040a0) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb06rom5.bin", 0x00000, 0x80000, CRC(97b80b5d) SHA1(871a3f90b6bc6c0b0aa2d1b863149bd17c8403bc) )
	ROM_LOAD16_BYTE( "fb06rom6.bin", 0x00001, 0x80000, CRC(94f66c23) SHA1(c08ff1ffaf75621ab28cd7b7f43d3744614cd5c3) )
ROM_END

ROM_START( fb6d )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "f06d17lt.bin", 0x00000, 0x40000, CRC(e222e19f) SHA1(1cd7bc2b802ece74735ec2a794ab5be041c24189) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb06rom2.bin", 0x00000, 0x40000, CRC(f1adbcd5) SHA1(90a8830d000eb634c2db8a09431daba6cdcb2d34) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb06rom3.bin", 0x00000, 0x40000, CRC(63159a3a) SHA1(77e5801506ea58df73c406c8675dc8c06ba92313) )
	ROM_LOAD16_BYTE( "fb06rom4.bin", 0x00001, 0x40000, CRC(e4f4f04e) SHA1(412cfec7235455c09cffde5ca05c3e2fe4a040a0) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb06rom5.bin", 0x00000, 0x80000, CRC(97b80b5d) SHA1(871a3f90b6bc6c0b0aa2d1b863149bd17c8403bc) )
	ROM_LOAD16_BYTE( "fb06rom6.bin", 0x00001, 0x80000, CRC(94f66c23) SHA1(c08ff1ffaf75621ab28cd7b7f43d3744614cd5c3) )
ROM_END

ROM_START( fb6d2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "f06d17r.bin", 0x00000, 0x40000, CRC(b3c1bb6f) SHA1(b8c46066a61ae48eb400014657dd80e7ef6de976) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb06rom2.bin", 0x00000, 0x40000, CRC(f1adbcd5) SHA1(90a8830d000eb634c2db8a09431daba6cdcb2d34) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb06rom3.bin", 0x00000, 0x40000, CRC(63159a3a) SHA1(77e5801506ea58df73c406c8675dc8c06ba92313) )
	ROM_LOAD16_BYTE( "fb06rom4.bin", 0x00001, 0x40000, CRC(e4f4f04e) SHA1(412cfec7235455c09cffde5ca05c3e2fe4a040a0) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb06rom5.bin", 0x00000, 0x80000, CRC(97b80b5d) SHA1(871a3f90b6bc6c0b0aa2d1b863149bd17c8403bc) )
	ROM_LOAD16_BYTE( "fb06rom6.bin", 0x00001, 0x80000, CRC(94f66c23) SHA1(c08ff1ffaf75621ab28cd7b7f43d3744614cd5c3) )
ROM_END


ROM_START( fb6v )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "f06v17r.bin", 0x00000, 0x40000, CRC(f65ef744) SHA1(1a4fb2b5d34b7466f398b115792a6f972c37e11e) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb06rom2.bin", 0x00000, 0x40000, CRC(f1adbcd5) SHA1(90a8830d000eb634c2db8a09431daba6cdcb2d34) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb06rom3.bin", 0x00000, 0x40000, CRC(63159a3a) SHA1(77e5801506ea58df73c406c8675dc8c06ba92313) )
	ROM_LOAD16_BYTE( "fb06rom4.bin", 0x00001, 0x40000, CRC(e4f4f04e) SHA1(412cfec7235455c09cffde5ca05c3e2fe4a040a0) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb06rom5.bin", 0x00000, 0x80000, CRC(97b80b5d) SHA1(871a3f90b6bc6c0b0aa2d1b863149bd17c8403bc) )
	ROM_LOAD16_BYTE( "fb06rom6.bin", 0x00001, 0x80000, CRC(94f66c23) SHA1(c08ff1ffaf75621ab28cd7b7f43d3744614cd5c3) )
ROM_END

ROM_START( fb6v2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "f06v17lt.bin", 0x00000, 0x40000, CRC(ac70303d) SHA1(c00a776b10142d478d617890d638f260fdc2c356) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb06rom2.bin", 0x00000, 0x40000, CRC(f1adbcd5) SHA1(90a8830d000eb634c2db8a09431daba6cdcb2d34) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb06rom3.bin", 0x00000, 0x40000, CRC(63159a3a) SHA1(77e5801506ea58df73c406c8675dc8c06ba92313) )
	ROM_LOAD16_BYTE( "fb06rom4.bin", 0x00001, 0x40000, CRC(e4f4f04e) SHA1(412cfec7235455c09cffde5ca05c3e2fe4a040a0) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb06rom5.bin", 0x00000, 0x80000, CRC(97b80b5d) SHA1(871a3f90b6bc6c0b0aa2d1b863149bd17c8403bc) )
	ROM_LOAD16_BYTE( "fb06rom6.bin", 0x00001, 0x80000, CRC(94f66c23) SHA1(c08ff1ffaf75621ab28cd7b7f43d3744614cd5c3) )
ROM_END

ROM_START( fb6v3 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "f06v17e.bin", 0x00000, 0x40000, CRC(fa42f143) SHA1(e410cc7ae1c86b540c5f573974ee68944fc51a3d))

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb06rom2.bin", 0x00000, 0x40000, CRC(f1adbcd5) SHA1(90a8830d000eb634c2db8a09431daba6cdcb2d34) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb06rom3.bin", 0x00000, 0x40000, CRC(63159a3a) SHA1(77e5801506ea58df73c406c8675dc8c06ba92313) )
	ROM_LOAD16_BYTE( "fb06rom4.bin", 0x00001, 0x40000, CRC(e4f4f04e) SHA1(412cfec7235455c09cffde5ca05c3e2fe4a040a0) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "fb06rom5.bin", 0x00000, 0x80000, CRC(97b80b5d) SHA1(871a3f90b6bc6c0b0aa2d1b863149bd17c8403bc) )
	ROM_LOAD16_BYTE( "fb06rom6.bin", 0x00001, 0x80000, CRC(94f66c23) SHA1(c08ff1ffaf75621ab28cd7b7f43d3744614cd5c3) )
ROM_END

ROM_START( fb6s )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "f06s17r.bin", 0x00000, 0x40000, CRC(679747d1) SHA1(ca702324c436d54f4c23350b1af4f0250915883c) )

	// this is a 'compact' board,  the standard roms don't seem correct

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb06rom2.bin", 0x00000, 0x40000, CRC(f1adbcd5) SHA1(90a8830d000eb634c2db8a09431daba6cdcb2d34) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb06rom3.bin", 0x00000, 0x40000, CRC(63159a3a) SHA1(77e5801506ea58df73c406c8675dc8c06ba92313) )
	ROM_LOAD16_BYTE( "fb06rom4.bin", 0x00001, 0x40000, CRC(e4f4f04e) SHA1(412cfec7235455c09cffde5ca05c3e2fe4a040a0) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	/* these should be different */
	ROM_LOAD16_BYTE( "fb06rom5_s.bin", 0x00000, 0x80000, NO_DUMP )
	ROM_LOAD16_BYTE( "fb06rom6_s.bin", 0x00001, 0x80000, NO_DUMP )
ROM_END

ROM_START( fb6s2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "f06s17lt.bin", 0x00000, 0x40000, CRC(2008a56f) SHA1(1dec4818c49cd63cc29fcb5abdab7a256731ae7b) )

	// this is a 'compact' board,  the standard roms don't seem correct

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "fb06rom2.bin", 0x00000, 0x40000, CRC(f1adbcd5) SHA1(90a8830d000eb634c2db8a09431daba6cdcb2d34) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "fb06rom3.bin", 0x00000, 0x40000, CRC(63159a3a) SHA1(77e5801506ea58df73c406c8675dc8c06ba92313) )
	ROM_LOAD16_BYTE( "fb06rom4.bin", 0x00001, 0x40000, CRC(e4f4f04e) SHA1(412cfec7235455c09cffde5ca05c3e2fe4a040a0) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	/* these should be different */
	ROM_LOAD16_BYTE( "fb06rom5_s.bin", 0x00000, 0x80000, NO_DUMP )
	ROM_LOAD16_BYTE( "fb06rom6_s.bin", 0x00001, 0x80000, NO_DUMP )
ROM_END

/* Special Edition */

ROM_START( fb6se )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "se6d14r.bin", 0x00000, 0x40000, CRC(70567bf1) SHA1(2e2bb317d558c4a8a008a695097f474b5e58ccf4) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "se06rom2.bin", 0x00000, 0x40000, CRC(f1adbcd5) SHA1(90a8830d000eb634c2db8a09431daba6cdcb2d34) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "se06rom3.bin", 0x00000, 0x80000, CRC(83224775) SHA1(fbdaf9df3d50776579699267ab9f84136b6e9321) )
	ROM_LOAD16_BYTE( "se06rom4.bin", 0x00001, 0x80000, CRC(21401db4) SHA1(05d7ee132b2aa99b3b5ae7477b8acd3a4550967e) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "se06rom5.bin", 0x00000, 0x80000, CRC(d7530334) SHA1(faa336f2d4a32006c2854a34fb365e89e86660ce) )
	ROM_LOAD16_BYTE( "se06rom6.bin", 0x00001, 0x80000, CRC(19f5873f) SHA1(c74a8b19faa754c77a50545a543e2b3069372046) )
ROM_END

ROM_START( fb6sed )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "se6d14lt.bin", 0x00000, 0x40000, CRC(97cf4951) SHA1(a750d61e4a92a79512cfbef138927581a1e5494c) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "se06rom2.bin", 0x00000, 0x40000, CRC(f1adbcd5) SHA1(90a8830d000eb634c2db8a09431daba6cdcb2d34) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "se06rom3.bin", 0x00000, 0x80000, CRC(83224775) SHA1(fbdaf9df3d50776579699267ab9f84136b6e9321) )
	ROM_LOAD16_BYTE( "se06rom4.bin", 0x00001, 0x80000, CRC(21401db4) SHA1(05d7ee132b2aa99b3b5ae7477b8acd3a4550967e) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "se06rom5.bin", 0x00000, 0x80000, CRC(d7530334) SHA1(faa336f2d4a32006c2854a34fb365e89e86660ce) )
	ROM_LOAD16_BYTE( "se06rom6.bin", 0x00001, 0x80000, CRC(19f5873f) SHA1(c74a8b19faa754c77a50545a543e2b3069372046) )
ROM_END

ROM_START( fb6sed2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "se6d14e.bin", 0x00000, 0x40000, CRC(e6e54c02) SHA1(f3c1ceb6ac551d2c9bcd244b57cdf0522768d99e) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "se06rom2.bin", 0x00000, 0x40000, CRC(f1adbcd5) SHA1(90a8830d000eb634c2db8a09431daba6cdcb2d34) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "se06rom3.bin", 0x00000, 0x80000, CRC(83224775) SHA1(fbdaf9df3d50776579699267ab9f84136b6e9321) )
	ROM_LOAD16_BYTE( "se06rom4.bin", 0x00001, 0x80000, CRC(21401db4) SHA1(05d7ee132b2aa99b3b5ae7477b8acd3a4550967e) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "se06rom5.bin", 0x00000, 0x80000, CRC(d7530334) SHA1(faa336f2d4a32006c2854a34fb365e89e86660ce) )
	ROM_LOAD16_BYTE( "se06rom6.bin", 0x00001, 0x80000, CRC(19f5873f) SHA1(c74a8b19faa754c77a50545a543e2b3069372046) )
ROM_END

ROM_START( fb6sev )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "se6v14r.bin", 0x00000, 0x40000,  CRC(8c5a93c9) SHA1(d101a05327e957ab83dc8a45aa005126da3a8fc6) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "se06rom2.bin", 0x00000, 0x40000, CRC(f1adbcd5) SHA1(90a8830d000eb634c2db8a09431daba6cdcb2d34) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "se06rom3.bin", 0x00000, 0x80000, CRC(83224775) SHA1(fbdaf9df3d50776579699267ab9f84136b6e9321) )
	ROM_LOAD16_BYTE( "se06rom4.bin", 0x00001, 0x80000, CRC(21401db4) SHA1(05d7ee132b2aa99b3b5ae7477b8acd3a4550967e) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "se06rom5.bin", 0x00000, 0x80000, CRC(d7530334) SHA1(faa336f2d4a32006c2854a34fb365e89e86660ce) )
	ROM_LOAD16_BYTE( "se06rom6.bin", 0x00001, 0x80000, CRC(19f5873f) SHA1(c74a8b19faa754c77a50545a543e2b3069372046) )
ROM_END

ROM_START( fb6sev2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "se6v14lt.bin", 0x00000, 0x40000, CRC(f5be2b37) SHA1(b3ff3ec456cbed064e5d05d58b4ff74d61b288dd) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "se06rom2.bin", 0x00000, 0x40000, CRC(f1adbcd5) SHA1(90a8830d000eb634c2db8a09431daba6cdcb2d34) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "se06rom3.bin", 0x00000, 0x80000, CRC(83224775) SHA1(fbdaf9df3d50776579699267ab9f84136b6e9321) )
	ROM_LOAD16_BYTE( "se06rom4.bin", 0x00001, 0x80000, CRC(21401db4) SHA1(05d7ee132b2aa99b3b5ae7477b8acd3a4550967e) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "se06rom5.bin", 0x00000, 0x80000, CRC(d7530334) SHA1(faa336f2d4a32006c2854a34fb365e89e86660ce) )
	ROM_LOAD16_BYTE( "se06rom6.bin", 0x00001, 0x80000, CRC(19f5873f) SHA1(c74a8b19faa754c77a50545a543e2b3069372046) )
ROM_END

ROM_START( fb6sev3 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "se6v14e.bin", 0x00000, 0x40000, CRC(00b681ea) SHA1(050bbe532c6869f64af47a9deec4e12652676e1b) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "se06rom2.bin", 0x00000, 0x40000, CRC(f1adbcd5) SHA1(90a8830d000eb634c2db8a09431daba6cdcb2d34) )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "se06rom3.bin", 0x00000, 0x80000, CRC(83224775) SHA1(fbdaf9df3d50776579699267ab9f84136b6e9321) )
	ROM_LOAD16_BYTE( "se06rom4.bin", 0x00001, 0x80000, CRC(21401db4) SHA1(05d7ee132b2aa99b3b5ae7477b8acd3a4550967e) )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "se06rom5.bin", 0x00000, 0x80000, CRC(d7530334) SHA1(faa336f2d4a32006c2854a34fb365e89e86660ce) )
	ROM_LOAD16_BYTE( "se06rom6.bin", 0x00001, 0x80000, CRC(19f5873f) SHA1(c74a8b19faa754c77a50545a543e2b3069372046) )
ROM_END

ROM_START( version4 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "96d42r.bin", 0x00000, 0x40000, CRC(dab5706c) SHA1(9fc37b66942a5e7535b4590f132727d793f9d705) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "96rom2.bin", 0x00000, 0x40000, NO_DUMP )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "96rom3.bin", 0x00000, 0x40000, NO_DUMP )
	ROM_LOAD16_BYTE( "96rom4.bin", 0x00001, 0x40000, NO_DUMP)

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "96rom5.bin", 0x00000, 0x40000, NO_DUMP )
	ROM_LOAD16_BYTE( "96rom6.bin", 0x00001, 0x40000, NO_DUMP )
ROM_END

ROM_START( bugfever )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "bfd17r.bin", 0x00000, 0x80000, CRC(6fc33307) SHA1(fdb10bd3e463cac2f9050d2d37fdfba9ccee91dc) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "bfrom2.bin", 0x00000, 0x40000, NO_DUMP )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "bfrom3.bin", 0x00000, 0x80000, NO_DUMP )
	ROM_LOAD16_BYTE( "bfrom4.bin", 0x00001, 0x80000, NO_DUMP )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "bfrom5.bin", 0x00000, 0x80000, NO_DUMP )
	ROM_LOAD16_BYTE( "bfrom6.bin", 0x00001, 0x80000, NO_DUMP )
ROM_END

ROM_START( bugfeverd )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "bfd17e.bin", 0x00000, 0x80000, CRC(35324195) SHA1(ad290912556f4ddbc33667b3bce5d05f321870d0) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "bfrom2.bin", 0x00000, 0x40000, NO_DUMP )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "bfrom3.bin", 0x00000, 0x80000, NO_DUMP )
	ROM_LOAD16_BYTE( "bfrom4.bin", 0x00001, 0x80000, NO_DUMP )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "bfrom5.bin", 0x00000, 0x80000, NO_DUMP )
	ROM_LOAD16_BYTE( "bfrom6.bin", 0x00001, 0x80000, NO_DUMP )
ROM_END

ROM_START( bugfeverv )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "bfv17r.bin", 0x00000, 0x80000, CRC(8b6ee6f5) SHA1(981d60f04ab44ce8fc63019ac3e5b689aa80baf0) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "bfrom2.bin", 0x00000, 0x40000, NO_DUMP )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "bfrom3.bin", 0x00000, 0x80000, NO_DUMP )
	ROM_LOAD16_BYTE( "bfrom4.bin", 0x00001, 0x80000, NO_DUMP )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "bfrom5.bin", 0x00000, 0x80000, NO_DUMP )
	ROM_LOAD16_BYTE( "bfrom6.bin", 0x00001, 0x80000, NO_DUMP )
ROM_END

ROM_START( bugfeverv2 )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "bfv17e.bin", 0x00000, 0x80000, CRC(b9afd39a) SHA1(d331551f679b8694bf63812e5e1a54361c87c52a) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "bfrom2.bin", 0x00000, 0x40000, NO_DUMP )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "bfrom3.bin", 0x00000, 0x80000, NO_DUMP )
	ROM_LOAD16_BYTE( "bfrom4.bin", 0x00001, 0x80000, NO_DUMP )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "bfrom5.bin", 0x00000, 0x80000, NO_DUMP )
	ROM_LOAD16_BYTE( "bfrom6.bin", 0x00001, 0x80000, NO_DUMP )
ROM_END

ROM_START( dvisland )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "did14r.bin", 0x00000, 0x80000, CRC(28c3a7eb) SHA1(5389338ef42e05542e3ff052b2bbc918cf619874) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "dirom2.bin", 0x00000, 0x40000, NO_DUMP )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "dirom3.bin", 0x00000, 0x80000, NO_DUMP )
	ROM_LOAD16_BYTE( "dirom4.bin", 0x00001, 0x80000, NO_DUMP )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "dirom5.bin", 0x00000, 0x80000, NO_DUMP )
	ROM_LOAD16_BYTE( "dirom6.bin", 0x00001, 0x80000, NO_DUMP )
ROM_END

ROM_START( funriver )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "frd14r.bin", 0x00000, 0x80000, CRC(03ffabcc) SHA1(0e65be88dc4158f77082e5b50836197dd0e397da) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "frrom2.bin", 0x00000, 0x40000, NO_DUMP )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "frrom3.bin", 0x00000, 0x80000, NO_DUMP )
	ROM_LOAD16_BYTE( "frrom4.bin", 0x00001, 0x80000, NO_DUMP )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "frrom5.bin", 0x00000, 0x80000, NO_DUMP )
	ROM_LOAD16_BYTE( "frrom6.bin", 0x00001, 0x80000, NO_DUMP )
ROM_END

ROM_START( funriverv )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "frv14r.bin", 0x00000, 0x80000, CRC(5629d38e) SHA1(6404f70d94b1ec39d1df4e00c620eb5498d3ff83) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */
	ROM_LOAD( "frrom2.bin", 0x00000, 0x40000, NO_DUMP )

	ROM_REGION( 0x100000, "gfx1", 0 )
	ROM_LOAD16_BYTE( "frrom3.bin", 0x00000, 0x80000, NO_DUMP )
	ROM_LOAD16_BYTE( "frrom4.bin", 0x00001, 0x80000, NO_DUMP )

	ROM_REGION( 0x100000, "gfx2", 0 )
	ROM_LOAD16_BYTE( "frrom5.bin", 0x00000, 0x80000, NO_DUMP )
	ROM_LOAD16_BYTE( "frrom6.bin", 0x00001, 0x80000, NO_DUMP )
ROM_END

// diagnostics?
ROM_START( amclink )
	ROM_REGION( 0x80000, "maincpu", 0 ) /* Z80 Code */
	ROM_LOAD( "link22.bin", 0x00000, 0x1ffff, BAD_DUMP CRC(e1acc705) SHA1(eb5684a0924add44f64637c2610f4c9650b8f4d9) )

	ROM_REGION( 0x040000, "oki", ROMREGION_ERASE00 ) /* Samples */

	ROM_REGION( 0x100000, "gfx1", ROMREGION_ERASE00 )

	ROM_REGION( 0x100000, "gfx2", ROMREGION_ERASE00 )
ROM_END
#endif
//ROM_REGION( 0x80000, "user1", 0 ) /* Z80 Code */
//ROM_LOAD( "dummy.rom", 0x00000, 0x40000, CRC(1) SHA1(1) )

static DRIVER_INIT( sfbonus_common)
{
	sfbonus_tilemap_ram = auto_malloc(0x4000);
	state_save_register_global_pointer(machine, sfbonus_tilemap_ram , 0x4000);

	sfbonus_reel_ram = auto_malloc(0x0800);
	state_save_register_global_pointer(machine, sfbonus_reel_ram , 0x0800);

	sfbonus_reel2_ram = auto_malloc(0x0800);
	state_save_register_global_pointer(machine, sfbonus_reel2_ram , 0x0800);

	sfbonus_reel3_ram = auto_malloc(0x0800);
	state_save_register_global_pointer(machine, sfbonus_reel3_ram , 0x0800);

	sfbonus_reel4_ram = auto_malloc(0x0800);
	state_save_register_global_pointer(machine, sfbonus_reel4_ram , 0x0800);

	// hack, because the debugger is broken
	sfbonus_videoram = memory_region(machine,"debugram");
	if (!sfbonus_videoram) sfbonus_videoram = auto_malloc(0x10000);


	state_save_register_global_pointer(machine, sfbonus_videoram, 0x10000);

	// dummy.rom helper
	{
		UINT8 *ROM = memory_region(machine, "maincpu");
		int length = memory_region_length(machine, "maincpu");
		UINT8* ROM2 = memory_region(machine,"user1");

		if (ROM2)
		{
			printf("X %02x %02x %02x %02x %02x %02x %02x %02x\n", ROM[0x50], ROM[0x51], ROM[0x52], ROM[0x53], ROM[0x54], ROM[0x55],ROM[0x56],ROM[0x57]);

			{

				int x;
				int y;
				for (y=0;y<0x8;y++)
				{
					printf("@Echo Off\n");
					printf("a.exe ");
					for (x=0;x<0x20*0x8;x+=0x8)
					{
						printf("%02x %02x ", ROM[x+y], ROM2[x+y]);
					}
					printf("\n");
				}

			}

			{
				FILE *fp;
				char filename[256];
				sprintf(filename,"decr_%s", machine->gamedrv->name);
				fp=fopen(filename, "w+b");
				if (fp)
				{
					fwrite(ROM, length, 1, fp);
					fclose(fp);
				}
			}
		}
	}
}

static void sfbonus_bitswap( running_machine* machine,
						UINT8 xor0, UINT8 b00, UINT8 b01, UINT8 b02, UINT8 b03, UINT8 b04, UINT8 b05, UINT8 b06,UINT8 b07,
                        UINT8 xor1, UINT8 b10, UINT8 b11, UINT8 b12, UINT8 b13, UINT8 b14, UINT8 b15, UINT8 b16,UINT8 b17,
	                    UINT8 xor2, UINT8 b20, UINT8 b21, UINT8 b22, UINT8 b23, UINT8 b24, UINT8 b25, UINT8 b26,UINT8 b27,
		                UINT8 xor3, UINT8 b30, UINT8 b31, UINT8 b32, UINT8 b33, UINT8 b34, UINT8 b35, UINT8 b36,UINT8 b37,
			            UINT8 xor4, UINT8 b40, UINT8 b41, UINT8 b42, UINT8 b43, UINT8 b44, UINT8 b45, UINT8 b46,UINT8 b47,
				        UINT8 xor5, UINT8 b50, UINT8 b51, UINT8 b52, UINT8 b53, UINT8 b54, UINT8 b55, UINT8 b56,UINT8 b57,
				        UINT8 xor6, UINT8 b60, UINT8 b61, UINT8 b62, UINT8 b63, UINT8 b64, UINT8 b65, UINT8 b66,UINT8 b67,
				        UINT8 xor7, UINT8 b70, UINT8 b71, UINT8 b72, UINT8 b73, UINT8 b74, UINT8 b75, UINT8 b76,UINT8 b77 )
{

	int i;
	UINT8 *ROM = memory_region(machine, "maincpu");

	for(i=0;i<memory_region_length(machine, "maincpu");i++)
	{
		UINT8 x = ROM[i];

		switch(i & 7)
		{
			case 0: x = BITSWAP8(x^xor0, b00,b01,b02,b03,b04,b05,b06,b07); break;
			case 1: x = BITSWAP8(x^xor1, b10,b11,b12,b13,b14,b15,b16,b17); break;
			case 2: x = BITSWAP8(x^xor2, b20,b21,b22,b23,b24,b25,b26,b27); break;
			case 3: x = BITSWAP8(x^xor3, b30,b31,b32,b33,b34,b35,b36,b37); break;
			case 4: x = BITSWAP8(x^xor4, b40,b41,b42,b43,b44,b45,b46,b47); break;
			case 5: x = BITSWAP8(x^xor5, b50,b51,b52,b53,b54,b55,b56,b57); break;
			case 6: x = BITSWAP8(x^xor6, b60,b61,b62,b63,b64,b65,b66,b67); break;
			case 7: x = BITSWAP8(x^xor7, b70,b71,b72,b73,b74,b75,b76,b77); break;
		}

		ROM[i] = x;
	}
	DRIVER_INIT_CALL(sfbonus_common);
}



static DRIVER_INIT(sfbonus) { sfbonus_bitswap(machine,    0x2a, 1,3,7,6,5,2,0,4, 0xe4, 3,7,6,5,2,0,4,1, 0x2d, 4,1,3,7,6,5,2,0, 0xba, 4,3,0,2,1,7,6,5, 0x30, 2,1,7,6,5,0,3,4, 0xf1, 2,7,6,5,1,3,4,0, 0x3d, 2,1,4,7,6,5,3,0, 0xba, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(act2000) { sfbonus_bitswap(machine,    0x25, 1,2,7,6,5,4,3,0, 0xE6, 1,7,6,5,4,3,0,2, 0x20, 2,4,1,7,6,5,0,3, 0xBF, 0,3,1,2,4,7,6,5, 0x2E, 1,3,7,6,5,2,0,4, 0xE0, 3,7,6,5,2,0,4,1, 0x2D, 4,1,2,7,6,5,0,3, 0xB2, 2,0,4,1,3,7,6,5); }
static DRIVER_INIT(dblchal) { sfbonus_bitswap(machine,    0x3D, 0,3,7,6,5,2,1,4, 0xF3, 3,7,6,5,1,0,4,2, 0x3D, 2,0,1,7,6,5,3,4, 0xA8, 3,4,2,0,1,7,6,5, 0x3D, 2,3,7,6,5,1,0,4, 0xEF, 2,7,6,5,1,0,3,4, 0x3A, 4,2,3,7,6,5,1,0, 0xBA, 2,4,1,0,3,7,6,5); }
static DRIVER_INIT(hldspin1) { sfbonus_bitswap(machine,   0x21, 0,2,7,6,5,4,3,1, 0xe1, 1,7,6,5,4,3,2,0, 0x31, 1,4,3,7,6,5,2,0, 0xbc, 0,3,4,2,1,7,6,5, 0x24, 4,3,7,6,5,2,0,1, 0xf8, 3,7,6,5,2,0,1,4, 0x39, 1,4,2,7,6,5,0,3, 0xaf, 0,3,2,1,4,7,6,5); }
static DRIVER_INIT(hldspin2) { sfbonus_bitswap(machine,   0x21, 1,3,7,6,5,0,4,2, 0xfe, 2,7,6,5,1,0,4,3, 0x33, 1,0,3,7,6,5,2,4, 0xa6, 1,0,4,3,2,7,6,5, 0x37, 0,1,7,6,5,3,2,4, 0xfe, 2,7,6,5,1,0,4,3, 0x36, 1,0,4,7,6,5,3,2, 0xa2, 1,0,2,4,3,7,6,5); }
static DRIVER_INIT(pickwin) { sfbonus_bitswap(machine,    0x20, 1,3,7,6,5,2,4,0, 0xfa, 2,7,6,5,4,0,1,3, 0x37, 1,0,3,7,6,5,2,4, 0xb0, 4,0,1,3,2,7,6,5, 0x34, 0,1,7,6,5,3,2,4, 0xef, 3,7,6,5,2,0,1,4, 0x27, 1,0,4,7,6,5,3,2, 0xb0, 4,0,1,3,2,7,6,5); }
static DRIVER_INIT(robadv) { sfbonus_bitswap(machine,     0x31, 0,3,7,6,5,2,1,4, 0xe0, 1,7,6,5,3,2,4,0, 0x2f, 4,0,2,7,6,5,3,1, 0xa7, 1,0,3,4,2,7,6,5, 0x33, 1,3,7,6,5,2,0,4, 0xed, 2,7,6,5,1,4,3,0, 0x34, 4,1,3,7,6,5,2,0, 0xaf, 2,0,4,1,3,7,6,5); }
static DRIVER_INIT(anibonus) { sfbonus_bitswap(machine,   0x33, 0,3,7,6,5,2,1,4, 0xe7, 2,7,6,5,3,4,1,0, 0x3a, 4,2,3,7,6,5,1,0, 0xa8, 3,4,2,0,1,7,6,5, 0x3d, 2,3,7,6,5,1,0,4, 0xff, 3,7,6,5,1,0,2,4, 0x3a, 4,2,3,7,6,5,1,0, 0xbe, 3,4,1,0,2,7,6,5); }
static DRIVER_INIT(pirpok2) { sfbonus_bitswap(machine,    0x26, 1,2,7,6,5,4,3,0, 0xf6, 1,7,6,5,4,3,0,2, 0x29, 4,0,1,7,6,5,2,3, 0xad, 0,3,1,2,4,7,6,5, 0x2e, 1,3,7,6,5,2,0,4, 0xe0, 3,7,6,5,2,0,4,1, 0x39, 4,1,2,7,6,5,0,3, 0xb2, 2,0,4,1,3,7,6,5); }
static DRIVER_INIT(tighook) { sfbonus_bitswap(machine,    0x33, 0,1,7,6,5,2,3,4, 0xf3, 3,7,6,5,1,0,4,2, 0x2e, 4,0,2,7,6,5,3,1, 0xa7, 1,0,4,2,3,7,6,5, 0x2d, 1,2,7,6,5,3,4,0, 0xff, 2,7,6,5,1,0,3,4, 0x27, 1,0,2,7,6,5,3,4, 0xa7, 1,0,4,2,3,7,6,5); }
static DRIVER_INIT(sfruitb) { sfbonus_bitswap(machine,    0x3e, 2,1,7,6,5,4,3,0, 0xfd, 1,7,6,5,0,3,2,4, 0x37, 4,1,3,7,6,5,2,0, 0xac, 2,0,4,1,3,7,6,5, 0x35, 2,3,7,6,5,1,0,4, 0xf6, 3,7,6,5,2,0,1,4, 0x37, 4,1,3,7,6,5,2,0, 0xb9, 0,3,4,1,2,7,6,5); }
static DRIVER_INIT(fb2gen) { sfbonus_bitswap(machine,     0x35, 0,3,7,6,5,2,1,4, 0xe8, 2,7,6,5,4,3,1,0, 0x23, 4,3,2,7,6,5,1,0, 0xb8, 2,1,4,0,3,7,6,5, 0x2d, 0,1,7,6,5,4,2,3, 0xf8, 2,7,6,5,1,4,3,0, 0x23, 4,0,3,7,6,5,2,1, 0xb8, 2,1,4,0,3,7,6,5); }
static DRIVER_INIT(fb2nd) { sfbonus_bitswap(machine,      0x2f, 0,2,7,6,5,3,4,1, 0xff, 2,7,6,5,3,0,4,1, 0x3e, 4,0,1,7,6,5,2,3, 0xad, 3,0,4,1,2,7,6,5, 0x35, 4,3,7,6,5,1,0,2, 0xfd, 4,7,6,5,3,1,2,0, 0x3a, 4,1,2,7,6,5,3,0, 0xbd, 3,4,2,0,1,7,6,5); }
static DRIVER_INIT(fb4) { sfbonus_bitswap(machine,        0x37, 1,2,7,6,5,4,3,0, 0xeb, 1,7,6,5,4,0,2,3, 0x2d, 4,0,2,7,6,5,3,1, 0xbd, 2,0,4,1,3,7,6,5, 0x29, 4,1,7,6,5,2,3,0, 0xff, 1,7,6,5,2,3,0,4, 0x3f, 1,0,4,7,6,5,3,2, 0xae, 2,3,0,4,1,7,6,5); }
static DRIVER_INIT(ch2000) { sfbonus_bitswap(machine,     0x29, 2,3,7,6,5,0,4,1, 0xfe, 2,7,6,5,1,0,3,4, 0x33, 0,1,3,7,6,5,2,4, 0xa6, 1,0,3,4,2,7,6,5, 0x25, 4,1,7,6,5,3,2,0, 0xfe, 2,7,6,5,1,0,3,4, 0x35, 0,1,4,7,6,5,3,2, 0xbe, 1,0,4,2,3,7,6,5); }
static DRIVER_INIT(anithunt) { sfbonus_bitswap(machine,   0x39, 1,2,7,6,5,4,3,0, 0xe7, 4,7,6,5,0,3,1,2, 0x33, 0,2,1,7,6,5,4,3, 0xb3, 0,3,4,2,1,7,6,5, 0x2a, 1,3,7,6,5,2,0,4, 0xe4, 3,7,6,5,2,0,4,1, 0x2d, 4,1,3,7,6,5,2,0, 0xb6, 0,3,2,1,4,7,6,5); }
static DRIVER_INIT(abnudge) { sfbonus_bitswap(machine,    0x33, 0,3,7,6,5,2,1,4, 0xff, 3,7,6,5,1,0,4,2, 0x36, 4,2,3,7,6,5,1,0, 0xa8, 3,2,4,0,1,7,6,5, 0x2c, 0,1,7,6,5,2,4,3, 0xff, 3,7,6,5,1,0,4,2, 0x26, 2,4,3,7,6,5,1,0, 0xbe, 4,1,3,0,2,7,6,5); }
static DRIVER_INIT(pir2001) { sfbonus_bitswap(machine,    0x3a, 1,2,7,6,5,4,3,0, 0xfa, 3,7,6,5,2,0,4,1, 0x33, 4,1,3,7,6,5,2,0, 0xa8, 2,0,4,1,3,7,6,5, 0x2a, 2,4,7,6,5,0,3,1, 0xf7, 1,7,6,5,4,3,0,2, 0x27, 4,1,2,7,6,5,0,3, 0xaf, 0,3,2,4,1,7,6,5); }
static DRIVER_INIT(pir2002) { sfbonus_bitswap(machine,    0x30, 3,2,7,6,5,4,0,1, 0xec, 2,7,6,5,4,0,1,3, 0x2d, 1,4,3,7,6,5,2,0, 0xa6, 4,0,1,3,2,7,6,5, 0x20, 4,1,7,6,5,2,3,0, 0xf9, 2,7,6,5,4,3,0,1, 0x3a, 4,1,2,7,6,5,0,3, 0xb7, 1,0,3,2,4,7,6,5); }
static DRIVER_INIT(classice) { sfbonus_bitswap(machine,   0x3f, 2,0,7,6,5,4,3,1, 0xe9, 2,7,6,5,4,3,1,0, 0x22, 2,1,0,7,6,5,4,3, 0xab, 4,3,2,0,1,7,6,5, 0x3e, 2,1,7,6,5,4,3,0, 0xeb, 2,7,6,5,4,3,0,1, 0x22, 0,2,1,7,6,5,4,3, 0xad, 4,3,0,2,1,7,6,5); }
static DRIVER_INIT(seawld) { sfbonus_bitswap(machine,     0x3c, 1,2,7,6,5,4,3,0, 0xea, 2,7,6,5,4,3,0,1, 0x24, 0,2,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 1,0,7,6,5,4,3,2, 0xec, 1,7,6,5,4,3,2,0, 0x23, 1,0,2,7,6,5,4,3, 0xa9, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(moneymac) { sfbonus_bitswap(machine,   0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x23, 0,2,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 0,1,7,6,5,4,3,2, 0xeb, 0,7,6,5,4,3,2,1, 0x25, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(atworld) { sfbonus_bitswap(machine,    0x3c, 1,0,7,6,5,4,3,2, 0xef, 0,7,6,5,4,3,2,1, 0x26, 1,0,2,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3a, 0,1,7,6,5,4,3,2, 0xe8, 1,7,6,5,4,3,0,2, 0x22, 0,1,2,7,6,5,4,3, 0xa9, 4,3,2,1,0,7,6,5); }
static DRIVER_INIT(fruitcar) { sfbonus_bitswap(machine,   0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x21, 2,1,0,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3f, 0,1,7,6,5,4,3,2, 0xee, 1,7,6,5,4,3,0,2, 0x25, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(act2000v) { sfbonus_bitswap(machine,   0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x23, 2,1,0,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 0,1,7,6,5,4,3,2, 0xe9, 1,7,6,5,4,3,0,2, 0x21, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(ch2000v) { sfbonus_bitswap(machine,    0x3c, 1,2,7,6,5,4,3,0, 0xea, 2,7,6,5,4,3,0,1, 0x22, 2,0,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3f, 1,0,7,6,5,4,3,2, 0xee, 1,7,6,5,4,3,0,2, 0x23, 1,0,2,7,6,5,4,3, 0xa9, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(ch2000v2) { sfbonus_bitswap(machine,   0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x23, 2,0,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3e, 2,1,7,6,5,4,3,0, 0xec, 0,7,6,5,4,3,2,1, 0x21, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(ch2000d) { sfbonus_bitswap(machine,    0x38, 0,2,7,6,5,4,3,1, 0xed, 0,7,6,5,4,3,2,1, 0x25, 2,0,1,7,6,5,4,3, 0xa9, 4,3,0,1,2,7,6,5, 0x3c, 0,1,7,6,5,4,3,2, 0xed, 1,7,6,5,4,3,0,2, 0x25, 2,0,1,7,6,5,4,3, 0xae, 4,3,1,2,0,7,6,5); }
static DRIVER_INIT(ch2000v3) { sfbonus_bitswap(machine,   0x3c, 1,2,7,6,5,4,3,0, 0xea, 2,7,6,5,4,3,0,1, 0x22, 2,0,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3f, 1,0,7,6,5,4,3,2, 0xee, 1,7,6,5,4,3,0,2, 0x23, 1,0,2,7,6,5,4,3, 0xa9, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(ch2000c) { sfbonus_bitswap(machine,    0x29, 2,3,7,6,5,0,4,1, 0xfe, 2,7,6,5,1,0,3,4, 0x33, 0,1,3,7,6,5,2,4, 0xa6, 1,0,3,4,2,7,6,5, 0x25, 4,1,7,6,5,3,2,0, 0xfe, 2,7,6,5,1,0,3,4, 0x35, 0,1,4,7,6,5,3,2, 0xbe, 1,0,4,2,3,7,6,5); }
static DRIVER_INIT(classiced) { sfbonus_bitswap(machine,  0x38, 0,2,7,6,5,4,3,1, 0xea, 2,7,6,5,4,3,0,1, 0x24, 2,1,0,7,6,5,4,3, 0xaa, 4,3,2,0,1,7,6,5, 0x3e, 1,0,7,6,5,4,3,2, 0xe8, 0,7,6,5,4,3,1,2, 0x24, 2,1,0,7,6,5,4,3, 0xa8, 4,3,0,2,1,7,6,5); }
static DRIVER_INIT(classiced3) { sfbonus_bitswap(machine, 0x3b, 2,1,7,6,5,4,3,0, 0xea, 2,7,6,5,4,3,0,1, 0x24, 2,1,0,7,6,5,4,3, 0xaa, 4,3,2,0,1,7,6,5, 0x3e, 1,0,7,6,5,4,3,2, 0xe8, 0,7,6,5,4,3,1,2, 0x24, 2,1,0,7,6,5,4,3, 0xae, 4,3,1,0,2,7,6,5); }
static DRIVER_INIT(classicev) { sfbonus_bitswap(machine,  0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x22, 2,0,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3a, 2,1,7,6,5,4,3,0, 0xea, 2,7,6,5,4,3,1,0, 0x22, 2,1,0,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(classicev3) { sfbonus_bitswap(machine, 0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x22, 2,0,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3f, 2,1,7,6,5,4,3,0, 0xe9, 2,7,6,5,4,3,1,0, 0x22, 2,1,0,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(seawlda) { sfbonus_bitswap(machine,    0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x22, 0,2,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3a, 0,1,7,6,5,4,3,2, 0xea, 2,7,6,5,4,3,1,0, 0x22, 2,1,0,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(atworlda) { sfbonus_bitswap(machine,   0x3c, 1,0,7,6,5,4,3,2, 0xef, 0,7,6,5,4,3,2,1, 0x26, 1,0,2,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 0,1,7,6,5,4,3,2, 0xec, 1,7,6,5,4,3,0,2, 0x22, 0,1,2,7,6,5,4,3, 0xa9, 4,3,2,1,0,7,6,5); }
static DRIVER_INIT(act2000v2) { sfbonus_bitswap(machine,  0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x21, 2,1,0,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3a, 0,1,7,6,5,4,3,2, 0xe9, 1,7,6,5,4,3,0,2, 0x21, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(act2000v3) { sfbonus_bitswap(machine,  0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x23, 2,1,0,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 0,1,7,6,5,4,3,2, 0xe9, 1,7,6,5,4,3,0,2, 0x21, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(act2000d) { sfbonus_bitswap(machine,   0x3d, 0,2,7,6,5,4,3,1, 0xef, 1,7,6,5,4,3,2,0, 0x27, 0,2,1,7,6,5,4,3, 0xad, 4,3,0,1,2,7,6,5, 0x3b, 2,1,7,6,5,4,3,0, 0xed, 0,7,6,5,4,3,2,1, 0x27, 0,2,1,7,6,5,4,3, 0xaa, 4,3,1,2,0,7,6,5); }
static DRIVER_INIT(anibonus3) { sfbonus_bitswap(machine,  0x33, 0,3,7,6,5,2,1,4, 0xff, 3,7,6,5,1,0,4,2, 0x36, 4,2,3,7,6,5,1,0, 0xa8, 3,2,4,0,1,7,6,5, 0x2c, 0,1,7,6,5,2,4,3, 0xff, 3,7,6,5,1,0,4,2, 0x26, 2,4,3,7,6,5,1,0, 0xbe, 4,1,3,0,2,7,6,5); }
static DRIVER_INIT(fruitcar2) { sfbonus_bitswap(machine,  0x33, 0,3,7,6,5,2,1,4, 0xff, 3,7,6,5,1,0,4,2, 0x36, 4,2,3,7,6,5,1,0, 0xa8, 3,2,4,0,1,7,6,5, 0x2c, 0,1,7,6,5,2,4,3, 0xff, 3,7,6,5,1,0,4,2, 0x26, 2,4,3,7,6,5,1,0, 0xbe, 4,1,3,0,2,7,6,5); }
static DRIVER_INIT(fruitcar3) { sfbonus_bitswap(machine,  0x3b, 0,1,7,6,5,4,3,2, 0xef, 0,7,6,5,4,3,2,1, 0x21, 0,2,1,7,6,5,4,3, 0xa9, 4,3,0,1,2,7,6,5, 0x3d, 2,1,7,6,5,4,3,0, 0xed, 2,7,6,5,4,3,1,0, 0x21, 0,2,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5); }
static DRIVER_INIT(moneymacv) { sfbonus_bitswap(machine,  0x3c, 1,2,7,6,5,4,3,0, 0xea, 2,7,6,5,4,3,0,1, 0x23, 0,2,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 0,1,7,6,5,4,3,2, 0xeb, 0,7,6,5,4,3,2,1, 0x23, 1,0,2,7,6,5,4,3, 0xa9, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(moneymacd) { sfbonus_bitswap(machine,  0x3a, 1,0,7,6,5,4,3,2, 0xe9, 0,7,6,5,4,3,1,2, 0x26, 0,2,1,7,6,5,4,3, 0xaf, 4,3,1,2,0,7,6,5, 0x3d, 0,2,7,6,5,4,3,1, 0xe9, 0,7,6,5,4,3,1,2, 0x23, 0,1,2,7,6,5,4,3, 0xae, 4,3,2,0,1,7,6,5); }
static DRIVER_INIT(pirpok2d) { sfbonus_bitswap(machine,   0x3c, 0,1,7,6,5,4,3,2, 0xed, 0,7,6,5,4,3,2,1, 0x21, 1,0,2,7,6,5,4,3, 0xa9, 4,3,0,1,2,7,6,5, 0x38, 0,2,7,6,5,4,3,1, 0xed, 1,7,6,5,4,3,0,2, 0x21, 1,0,2,7,6,5,4,3, 0xae, 4,3,1,2,0,7,6,5); }
static DRIVER_INIT(pirpok2v) { sfbonus_bitswap(machine,   0x3c, 1,0,7,6,5,4,3,2, 0xef, 0,7,6,5,4,3,2,1, 0x23, 2,0,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3e, 2,1,7,6,5,4,3,0, 0xea, 2,7,6,5,4,3,1,0, 0x22, 0,1,2,7,6,5,4,3, 0xa9, 4,3,2,1,0,7,6,5); }
static DRIVER_INIT(pirpok2v2) { sfbonus_bitswap(machine,  0x3c, 1,0,7,6,5,4,3,2, 0xef, 0,7,6,5,4,3,2,1, 0x22, 2,0,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3a, 2,1,7,6,5,4,3,0, 0xea, 2,7,6,5,4,3,1,0, 0x22, 0,1,2,7,6,5,4,3, 0xa9, 4,3,2,1,0,7,6,5); }
static DRIVER_INIT(dblchald) { sfbonus_bitswap(machine,   0x3c, 0,1,7,6,5,4,3,2, 0xed, 0,7,6,5,4,3,2,1, 0x27, 0,2,1,7,6,5,4,3, 0xae, 4,3,1,0,2,7,6,5, 0x3b, 2,1,7,6,5,4,3,0, 0xea, 2,7,6,5,4,3,0,1, 0x27, 0,2,1,7,6,5,4,3, 0xae, 4,3,1,2,0,7,6,5); }
static DRIVER_INIT(dblchalv) { sfbonus_bitswap(machine,   0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x22, 2,0,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3f, 1,0,7,6,5,4,3,2, 0xec, 1,7,6,5,4,3,0,2, 0x21, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(abnudged) { sfbonus_bitswap(machine,   0x3b, 0,1,7,6,5,4,3,2, 0xef, 0,7,6,5,4,3,2,1, 0x21, 0,2,1,7,6,5,4,3, 0xa9, 4,3,0,1,2,7,6,5, 0x3d, 2,1,7,6,5,4,3,0, 0xed, 2,7,6,5,4,3,1,0, 0x21, 0,2,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5); }
static DRIVER_INIT(abnudgev) { sfbonus_bitswap(machine,   0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x21, 2,1,0,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3f, 0,1,7,6,5,4,3,2, 0xee, 1,7,6,5,4,3,0,2, 0x25, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(pickwind) { sfbonus_bitswap(machine,   0x3c, 0,1,7,6,5,4,3,2, 0xed, 0,7,6,5,4,3,2,1, 0x27, 0,2,1,7,6,5,4,3, 0xae, 4,3,1,0,2,7,6,5, 0x3b, 2,1,7,6,5,4,3,0, 0xe8, 0,7,6,5,4,3,1,2, 0x27, 0,2,1,7,6,5,4,3, 0xa9, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(pickwinv) { sfbonus_bitswap(machine,   0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x26, 2,0,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 1,0,7,6,5,4,3,2, 0xee, 1,7,6,5,4,3,0,2, 0x25, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(pickwinv2) { sfbonus_bitswap(machine,  0x3c, 1,2,7,6,5,4,3,0, 0xea, 2,7,6,5,4,3,0,1, 0x26, 2,0,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 1,0,7,6,5,4,3,2, 0xee, 1,7,6,5,4,3,0,2, 0x23, 1,0,2,7,6,5,4,3, 0xa9, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(anithuntd) { sfbonus_bitswap(machine,  0x3c, 0,1,7,6,5,4,3,2, 0xee, 0,7,6,5,4,3,2,1, 0x21, 0,2,1,7,6,5,4,3, 0xa9, 4,3,0,1,2,7,6,5, 0x3d, 2,1,7,6,5,4,3,0, 0xed, 2,7,6,5,4,3,1,0, 0x21, 0,2,1,7,6,5,4,3, 0xae, 4,3,1,2,0,7,6,5); }
static DRIVER_INIT(anithuntv) { sfbonus_bitswap(machine,  0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x23, 2,0,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 1,0,7,6,5,4,3,2, 0xe9, 1,7,6,5,4,3,0,2, 0x21, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(anibonusd) { sfbonus_bitswap(machine,  0x3b, 0,1,7,6,5,4,3,2, 0xef, 0,7,6,5,4,3,2,1, 0x21, 0,2,1,7,6,5,4,3, 0xa8, 4,3,0,1,2,7,6,5, 0x3d, 2,1,7,6,5,4,3,0, 0xed, 2,7,6,5,4,3,1,0, 0x21, 0,2,1,7,6,5,4,3, 0xaa, 4,3,1,2,0,7,6,5); }
static DRIVER_INIT(anibonusv) { sfbonus_bitswap(machine,  0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x21, 2,1,0,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3f, 0,1,7,6,5,4,3,2, 0xec, 1,7,6,5,4,3,0,2, 0x21, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(anibonusv3) { sfbonus_bitswap(machine, 0x3c, 1,2,7,6,5,4,3,0, 0xea, 2,7,6,5,4,3,0,1, 0x21, 2,1,0,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3f, 0,1,7,6,5,4,3,2, 0xee, 1,7,6,5,4,3,0,2, 0x23, 1,0,2,7,6,5,4,3, 0xa9, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(hldspin1d) { sfbonus_bitswap(machine,  0x38, 0,1,7,6,5,4,3,2, 0xef, 0,7,6,5,4,3,2,1, 0x27, 1,0,2,7,6,5,4,3, 0xa9, 4,3,0,1,2,7,6,5, 0x3e, 0,2,7,6,5,4,3,1, 0xeb, 1,7,6,5,4,3,0,2, 0x27, 1,0,2,7,6,5,4,3, 0xae, 4,3,1,2,0,7,6,5); }
static DRIVER_INIT(hldspin1v) { sfbonus_bitswap(machine,  0x3c, 1,2,7,6,5,4,3,0, 0xea, 2,7,6,5,4,3,0,1, 0x22, 2,0,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3f, 2,1,7,6,5,4,3,0, 0xed, 2,7,6,5,4,3,1,0, 0x26, 2,1,0,7,6,5,4,3, 0xa9, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(parrot3d) { sfbonus_bitswap(machine,   0x3b, 0,1,7,6,5,4,3,2, 0xef, 0,7,6,5,4,3,2,1, 0x27, 0,2,1,7,6,5,4,3, 0xad, 4,3,0,1,2,7,6,5, 0x3b, 2,1,7,6,5,4,3,0, 0xee, 2,7,6,5,4,3,1,0, 0x27, 0,2,1,7,6,5,4,3, 0xaa, 4,3,1,2,0,7,6,5); }
static DRIVER_INIT(parrot3v) { sfbonus_bitswap(machine,   0x3c, 1,0,7,6,5,4,3,2, 0xef, 0,7,6,5,4,3,2,1, 0x22, 2,0,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3f, 2,1,7,6,5,4,3,0, 0xed, 2,7,6,5,4,3,1,0, 0x26, 0,1,2,7,6,5,4,3, 0xa9, 4,3,2,1,0,7,6,5); }
static DRIVER_INIT(parrot3v2) { sfbonus_bitswap(machine,  0x3c, 1,0,7,6,5,4,3,2, 0xef, 0,7,6,5,4,3,2,1, 0x22, 2,0,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3f, 2,1,7,6,5,4,3,0, 0xe9, 2,7,6,5,4,3,1,0, 0x22, 0,1,2,7,6,5,4,3, 0xa9, 4,3,2,1,0,7,6,5); }
static DRIVER_INIT(sfruitbd) { sfbonus_bitswap(machine,   0x3e, 1,0,7,6,5,4,3,2, 0xed, 1,7,6,5,4,3,0,2, 0x25, 2,0,1,7,6,5,4,3, 0xa9, 4,3,0,1,2,7,6,5, 0x3c, 0,1,7,6,5,4,3,2, 0xed, 2,7,6,5,4,3,1,0, 0x25, 2,0,1,7,6,5,4,3, 0xae, 4,3,1,2,0,7,6,5); }
static DRIVER_INIT(sfruitbv) { sfbonus_bitswap(machine,   0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x25, 2,1,0,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 0,1,7,6,5,4,3,2, 0xec, 1,7,6,5,4,3,0,2, 0x21, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(sfruitbv2) { sfbonus_bitswap(machine,  0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x25, 2,1,0,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 0,1,7,6,5,4,3,2, 0xee, 1,7,6,5,4,3,0,2, 0x25, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(sfbonusd) { sfbonus_bitswap(machine,   0x3b, 0,1,7,6,5,4,3,2, 0xef, 1,7,6,5,4,3,0,2, 0x24, 2,1,0,7,6,5,4,3, 0xad, 4,3,0,1,2,7,6,5, 0x3e, 1,0,7,6,5,4,3,2, 0xeb, 2,7,6,5,4,3,1,0, 0x24, 2,1,0,7,6,5,4,3, 0xaa, 4,3,1,2,0,7,6,5); }
static DRIVER_INIT(sfbonusv) { sfbonus_bitswap(machine,   0x3c, 1,2,7,6,5,4,3,0, 0xea, 2,7,6,5,4,3,0,1, 0x25, 2,1,0,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 0,1,7,6,5,4,3,2, 0xee, 1,7,6,5,4,3,0,2, 0x23, 1,0,2,7,6,5,4,3, 0xa9, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(fb2ndv) { sfbonus_bitswap(machine,     0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x22, 2,0,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3a, 2,1,7,6,5,4,3,0, 0xec, 0,7,6,5,4,3,2,1, 0x21, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(fb2ndd) { sfbonus_bitswap(machine,     0x3b, 1,0,7,6,5,4,3,2, 0xeb, 1,7,6,5,4,3,0,2, 0x25, 2,0,1,7,6,5,4,3, 0xad, 4,3,0,1,2,7,6,5, 0x3c, 0,1,7,6,5,4,3,2, 0xeb, 2,7,6,5,4,3,1,0, 0x25, 2,0,1,7,6,5,4,3, 0xaa, 4,3,1,2,0,7,6,5); }
static DRIVER_INIT(robadv2v1) { sfbonus_bitswap(machine,  0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x21, 2,1,0,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3a, 0,1,7,6,5,4,3,2, 0xe9, 1,7,6,5,4,3,0,2, 0x21, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(robadv2v4) { sfbonus_bitswap(machine,  0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x23, 2,1,0,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 0,1,7,6,5,4,3,2, 0xe9, 1,7,6,5,4,3,0,2, 0x21, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(robadv2d) { sfbonus_bitswap(machine,   0x3c, 0,1,7,6,5,4,3,2, 0xe8, 0,7,6,5,4,3,1,2, 0x24, 2,1,0,7,6,5,4,3, 0xae, 4,3,1,0,2,7,6,5, 0x3e, 1,0,7,6,5,4,3,2, 0xed, 1,7,6,5,4,3,0,2, 0x24, 2,1,0,7,6,5,4,3, 0xa9, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(hldspin2d) { sfbonus_bitswap(machine,  0x3b, 0,1,7,6,5,4,3,2, 0xef, 0,7,6,5,4,3,2,1, 0x27, 1,0,2,7,6,5,4,3, 0xa9, 4,3,0,1,2,7,6,5, 0x3e, 0,2,7,6,5,4,3,1, 0xeb, 1,7,6,5,4,3,0,2, 0x27, 1,0,2,7,6,5,4,3, 0xab, 4,3,1,2,0,7,6,5); }
static DRIVER_INIT(hldspin2v) { sfbonus_bitswap(machine,  0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x22, 2,0,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3f, 2,1,7,6,5,4,3,0, 0xed, 2,7,6,5,4,3,1,0, 0x23, 2,1,0,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(tighookv) { sfbonus_bitswap(machine,   0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x26, 0,2,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 0,1,7,6,5,4,3,2, 0xec, 0,7,6,5,4,3,2,1, 0x21, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(tighookv2) { sfbonus_bitswap(machine,  0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x23, 0,2,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 0,1,7,6,5,4,3,2, 0xea, 0,7,6,5,4,3,2,1, 0x21, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(tighookd) { sfbonus_bitswap(machine,   0x3d, 0,1,7,6,5,4,3,2, 0xed, 1,7,6,5,4,3,0,2, 0x26, 2,1,0,7,6,5,4,3, 0xa9, 4,3,0,1,2,7,6,5, 0x3c, 1,0,7,6,5,4,3,2, 0xed, 2,7,6,5,4,3,1,0, 0x26, 2,1,0,7,6,5,4,3, 0xae, 4,3,1,2,0,7,6,5); }
static DRIVER_INIT(pir2001v) { sfbonus_bitswap(machine,   0x39, 1,0,7,6,5,4,3,2, 0xea, 0,7,6,5,4,3,2,1, 0x22, 2,0,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3f, 2,1,7,6,5,4,3,0, 0xed, 2,7,6,5,4,3,1,0, 0x23, 0,1,2,7,6,5,4,3, 0xac, 4,3,2,1,0,7,6,5); }
static DRIVER_INIT(pir2001v2) { sfbonus_bitswap(machine,  0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x22, 2,0,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x39, 1,0,7,6,5,4,3,2, 0xe9, 1,7,6,5,4,3,0,2, 0x21, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(pir2001d) { sfbonus_bitswap(machine,   0x3c, 0,1,7,6,5,4,3,2, 0xeb, 0,7,6,5,4,3,2,1, 0x27, 1,0,2,7,6,5,4,3, 0xa9, 4,3,0,1,2,7,6,5, 0x3e, 0,2,7,6,5,4,3,1, 0xeb, 1,7,6,5,4,3,0,2, 0x27, 1,0,2,7,6,5,4,3, 0xae, 4,3,1,2,0,7,6,5); }
static DRIVER_INIT(pir2002d) { sfbonus_bitswap(machine,   0x3d, 2,0,7,6,5,4,3,1, 0xef, 1,7,6,5,4,3,2,0, 0x27, 0,2,1,7,6,5,4,3, 0xae, 4,3,1,0,2,7,6,5, 0x3b, 2,1,7,6,5,4,3,0, 0xed, 0,7,6,5,4,3,2,1, 0x27, 0,2,1,7,6,5,4,3, 0xa9, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(pir2002v) { sfbonus_bitswap(machine,   0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x23, 2,0,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 1,0,7,6,5,4,3,2, 0xe9, 1,7,6,5,4,3,0,2, 0x21, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(pir2002v2) { sfbonus_bitswap(machine,  0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x26, 2,0,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 1,0,7,6,5,4,3,2, 0xec, 1,7,6,5,4,3,0,2, 0x21, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(fb2genv) { sfbonus_bitswap(machine,    0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x22, 2,0,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3f, 2,1,7,6,5,4,3,0, 0xea, 0,7,6,5,4,3,2,1, 0x21, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(fb2genv3) { sfbonus_bitswap(machine,   0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x22, 2,0,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3f, 2,1,7,6,5,4,3,0, 0xeb, 0,7,6,5,4,3,2,1, 0x25, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(fb2gend) { sfbonus_bitswap(machine,    0x3d, 2,0,7,6,5,4,3,1, 0xeb, 1,7,6,5,4,3,0,2, 0x25, 2,0,1,7,6,5,4,3, 0xad, 4,3,0,1,2,7,6,5, 0x3c, 0,1,7,6,5,4,3,2, 0xeb, 2,7,6,5,4,3,1,0, 0x25, 2,0,1,7,6,5,4,3, 0xac, 4,3,2,1,0,7,6,5); }
static DRIVER_INIT(fb4d) { sfbonus_bitswap(machine,       0x3d, 2,0,7,6,5,4,3,1, 0xeb, 1,7,6,5,4,3,0,2, 0x25, 2,0,1,7,6,5,4,3, 0xad, 4,3,2,1,0,7,6,5, 0x3c, 0,1,7,6,5,4,3,2, 0xeb, 2,7,6,5,4,3,1,0, 0x25, 2,0,1,7,6,5,4,3, 0xa9, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(fb4v) { sfbonus_bitswap(machine,       0x3c, 1,2,7,6,5,4,3,0, 0xea, 2,7,6,5,4,3,0,1, 0x22, 2,0,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3f, 2,1,7,6,5,4,3,0, 0xeb, 0,7,6,5,4,3,2,1, 0x23, 1,0,2,7,6,5,4,3, 0xa9, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(fb4v3) { sfbonus_bitswap(machine,      0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x23, 2,0,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3e, 2,1,7,6,5,4,3,0, 0xea, 2,7,6,5,4,3,1,0, 0x22, 2,1,0,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(fb5) { sfbonus_bitswap(machine,        0x20, 0,3,7,6,5,1,4,2, 0xf1, 1,7,6,5,3,2,4,0, 0x33, 2,3,1,7,6,5,0,4, 0xaf, 2,0,1,4,3,7,6,5, 0x2d, 2,4,7,6,5,1,0,3, 0xfb, 4,7,6,5,1,0,3,2, 0x34, 2,0,4,7,6,5,3,1, 0xb7, 1,0,3,2,4,7,6,5); }
static DRIVER_INIT(fb5d) { sfbonus_bitswap(machine,       0x3e, 2,1,7,6,5,4,3,0, 0xef, 1,7,6,5,4,3,2,0, 0x24, 2,1,0,7,6,5,4,3, 0xad, 4,3,0,1,2,7,6,5, 0x3e, 1,0,7,6,5,4,3,2, 0xeb, 2,7,6,5,4,3,1,0, 0x24, 2,1,0,7,6,5,4,3, 0xaa, 4,3,1,2,0,7,6,5); }
static DRIVER_INIT(fb5v) { sfbonus_bitswap(machine,       0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x22, 2,0,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3f, 1,0,7,6,5,4,3,2, 0xee, 1,7,6,5,4,3,0,2, 0x25, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(fb6) { sfbonus_bitswap(machine,        0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x23, 2,1,0,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 0,1,7,6,5,4,3,2, 0xe9, 1,7,6,5,4,3,0,2, 0x21, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(fb6d) { sfbonus_bitswap(machine,       0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x22, 2,1,0,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 0,1,7,6,5,4,3,2, 0xe9, 1,7,6,5,4,3,0,2, 0x25, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(fb6v) { sfbonus_bitswap(machine,       0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x23, 0,2,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 0,1,7,6,5,4,3,2, 0xed, 2,7,6,5,4,3,1,0, 0x23, 2,1,0,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(fb6v3) { sfbonus_bitswap(machine,      0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x26, 0,2,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 0,1,7,6,5,4,3,2, 0xea, 2,7,6,5,4,3,1,0, 0x22, 2,1,0,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(fb6s) { sfbonus_bitswap(machine,       0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x24, 0,2,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 0,1,7,6,5,4,3,2, 0xec, 0,7,6,5,4,3,2,1, 0x25, 1,0,2,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(version4) { sfbonus_bitswap(machine,   0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x26, 1,0,2,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 0,1,7,6,5,4,3,2, 0xec, 1,7,6,5,4,3,0,2, 0x22, 2,1,0,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(bugfever) { sfbonus_bitswap(machine,   0x3c, 1,2,7,6,5,4,3,0, 0xea, 2,7,6,5,4,3,0,1, 0x22, 2,1,0,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 0,1,7,6,5,4,3,2, 0xe9, 1,7,6,5,4,3,0,2, 0x23, 1,0,2,7,6,5,4,3, 0xa9, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(bugfeverd) { sfbonus_bitswap(machine,  0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x26, 1,0,2,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3a, 0,1,7,6,5,4,3,2, 0xe8, 1,7,6,5,4,3,0,2, 0x22, 2,1,0,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(bugfeverv) { sfbonus_bitswap(machine,  0x3c, 1,0,7,6,5,4,3,2, 0xef, 0,7,6,5,4,3,2,1, 0x22, 0,2,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3a, 0,1,7,6,5,4,3,2, 0xea, 2,7,6,5,4,3,1,0, 0x22, 0,1,2,7,6,5,4,3, 0xa9, 4,3,2,1,0,7,6,5); }
static DRIVER_INIT(bugfeverv2) { sfbonus_bitswap(machine, 0x3c, 1,2,7,6,5,4,3,0, 0xea, 2,7,6,5,4,3,0,1, 0x23, 0,2,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 0,1,7,6,5,4,3,2, 0xed, 2,7,6,5,4,3,1,0, 0x26, 2,1,0,7,6,5,4,3, 0xa9, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(dvisland) { sfbonus_bitswap(machine,   0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x21, 1,0,2,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 0,1,7,6,5,4,3,2, 0xe9, 1,7,6,5,4,3,0,2, 0x23, 2,1,0,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(funriver) { sfbonus_bitswap(machine,   0x3c, 1,2,7,6,5,4,3,0, 0xea, 2,7,6,5,4,3,0,1, 0x24, 0,2,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 0,1,7,6,5,4,3,2, 0xec, 0,7,6,5,4,3,2,1, 0x23, 1,0,2,7,6,5,4,3, 0xa9, 4,3,0,1,2,7,6,5); }
static DRIVER_INIT(funriverv) { sfbonus_bitswap(machine,  0x39, 1,2,7,6,5,4,3,0, 0xef, 2,7,6,5,4,3,0,1, 0x26, 0,2,1,7,6,5,4,3, 0xa8, 4,3,1,2,0,7,6,5, 0x3b, 0,1,7,6,5,4,3,2, 0xea, 2,7,6,5,4,3,1,0, 0x22, 2,1,0,7,6,5,4,3, 0xac, 4,3,0,1,2,7,6,5); }



/*
            case 0: x = BITSWAP8(x^0xff, 7,6,5,4,3,2,1,0
            case 1: x = BITSWAP8(x^0xff, 7,6,5,4,3,2,1,0
            case 2: x = BITSWAP8(x^0xff, 7,6,5,4,3,2,1,0
            case 3: x = BITSWAP8(x^0xff, 7,6,5,4,3,2,1,0
            case 4: x = BITSWAP8(x^0xff, 7,6,5,4,3,2,1,0
            case 5: x = BITSWAP8(x^0xff, 7,6,5,4,3,2,1,0
            case 6: x = BITSWAP8(x^0xff, 7,6,5,4,3,2,1,0
            case 7: x = BITSWAP8(x^0xff, 7,6,5,4,3,2,1,0

*/

GAME( 2004, sfbonus,     0,        sfbonus,    amcoetype2,    sfbonus, ROT0,  "Amcoe", "Skill Fruit Bonus (Version 1.6)", 0)

GAME( 2004, sfbonusa,    sfbonus,  sfbonus,    amcoetype2,    sfbonus, ROT0,  "Amcoe", "Skill Fruit Bonus (Version 1.7)", 0)

GAME( 2004, sfbonusb,    sfbonus,  sfbonus,    amcoetype2,    sfbonus, ROT0,  "Amcoe", "Skill Fruit Bonus (Version 1.9R, set 1)", 0)
GAME( 2004, sfbonusd,    sfbonus,  sfbonus,    amcoetype1,    sfbonusd,ROT0,  "Amcoe", "Skill Fruit Bonus (Version 1.9R, set 2)", 0)
GAME( 2004, sfbonusv,    sfbonus,  sfbonus,    amcoetype1,    sfbonusv,ROT0,  "Amcoe", "Skill Fruit Bonus (Version 1.9R Dual)", 0)

GAME( 2004, parrot3,     0,        sfbonus,    amcoetype2,    pirpok2,  ROT0,  "Amcoe", "Parrot Poker III (Version 2.4)", 0)

GAME( 2004, parrot3b,    parrot3,  sfbonus,    amcoetype2,    pirpok2,  ROT0,  "Amcoe", "Parrot Poker III (Version 2.6R, set 1)", 0)
GAME( 2004, parrot3d,    parrot3,  sfbonus,    amcoetype1,    parrot3d, ROT0,  "Amcoe", "Parrot Poker III (Version 2.6R, set 2)", 0)
GAME( 2004, parrot3v2,   parrot3,  sfbonus,    amcoetype1,    parrot3v2,ROT0,  "Amcoe", "Parrot Poker III (Version 2.6R Dual)", 0)

GAME( 2004, parrot3v,    parrot3,  sfbonus,    amcoetype1,    parrot3v, ROT0,  "Amcoe", "Parrot Poker III (Version 2.6E Dual)", 0)

GAME( 2000, hldspin1,    0,        sfbonus,    amcoetype2,    hldspin1, ROT0,  "Amcoe", "Hold & Spin I (Version 2.5T)", 0)

GAME( 2000, hldspin1b,   hldspin1, sfbonus,    amcoetype2,    hldspin1, ROT0,  "Amcoe", "Hold & Spin I (Version 2.7T, set 1)", 0)
GAME( 2000, hldspin1d,   hldspin1, sfbonus,    amcoetype1,    hldspin1d, ROT0, "Amcoe", "Hold & Spin I (Version 2.7T, set 2)", 0)
GAME( 2000, hldspin1v,   hldspin1, sfbonus,    amcoetype1,    hldspin1v, ROT0, "Amcoe", "Hold & Spin I (Version 2.7T Dual)", 0)

GAME( 2000, hldspin2,    0,        sfbonus,    amcoetype2,    hldspin2, ROT0,  "Amcoe", "Hold & Spin II (Version 2.6)", 0)

GAME( 2000, hldspin2b,   hldspin2, sfbonus,    amcoetype2,    hldspin2, ROT0,  "Amcoe", "Hold & Spin II (Version 2.8R, set 1)", 0)
GAME( 2000, hldspin2d,   hldspin2, sfbonus,    amcoetype1,    hldspin2d, ROT0, "Amcoe", "Hold & Spin II (Version 2.8R, set 2)", 0) // some text corruption on first reset (MIN PLAY etc. real game bug?)
GAME( 2000, hldspin2v,   hldspin2, sfbonus,    amcoetype1,    hldspin2v, ROT0, "Amcoe", "Hold & Spin II (Version 2.8R Dual)", 0)  // some text corruption on first reset (MIN PLAY etc. real game bug?)

GAME( 2003, fcnudge,     0,        sfbonus,    amcoetype2,    abnudge,  ROT0,  "Amcoe", "Fruit Carnival Nudge (Version 1.7)", 0)
GAME( 2003, fcnudge2,    fcnudge,  sfbonus,    amcoetype1,    fruitcar, ROT0,  "Amcoe", "Fruit Carnival Nudge (Version 2.1 Dual)", 0)
GAME( 2003, fcnudge3,    fcnudge,  sfbonus,    amcoetype2,    fruitcar2,ROT0,  "Amcoe", "Fruit Carnival Nudge (Version 2.0, set 1)", 0)
GAME( 2003, fcnudge4,    fcnudge,  sfbonus,    amcoetype1,    fruitcar3,ROT0,  "Amcoe", "Fruit Carnival Nudge (Version 2.0, set 2)", 0)

GAME( 2001, pickwin,     0,        sfbonus,    amcoetype2,    pickwin,  ROT0,  "Amcoe", "Pick & Win (Version 2.5T)", 0)
GAME( 2001, pickwina,    pickwin,  sfbonus,    amcoetype2,    pickwin,  ROT0,  "Amcoe", "Pick & Win (Version 2.6)", 0)

GAME( 2001, pickwinb2,   pickwin,  sfbonus,    amcoetype2,    pickwin,  ROT0,  "Amcoe", "Pick & Win (Version 2.9R, set 1)", 0)
GAME( 2001, pickwind2,   pickwin,  sfbonus,    amcoetype1,    pickwind, ROT0,  "Amcoe", "Pick & Win (Version 2.9R, set 2)", 0)
GAME( 2001, pickwinv3,   pickwin,  sfbonus,    amcoetype1,    pickwinv, ROT0,  "Amcoe", "Pick & Win (Version 2.9R Dual)", 0)

GAME( 2001, pickwinb,    pickwin,  sfbonus,    amcoetype2,    pickwin,  ROT0,  "Amcoe", "Pick & Win (Version 2.8T, set 1)", 0)
GAME( 2001, pickwind,    pickwin,  sfbonus,    amcoetype1,    pickwind, ROT0,  "Amcoe", "Pick & Win (Version 2.8T, set 2)", 0)
GAME( 2001, pickwinv,    pickwin,  sfbonus,    amcoetype1,    pickwinv, ROT0,  "Amcoe", "Pick & Win (Version 2.8T, Dual)", 0)

GAME( 2001, pickwinv2,   pickwin,  sfbonus,    amcoetype1,    pickwinv2,ROT0,  "Amcoe", "Pick & Win (Version 2.9E Dual)", 0)


GAME( 2004, tighook,     0,        sfbonus,    amcoetype1,    tighook,  ROT0,  "Amcoe", "Tiger Hook (Version 1.7XT)", 0)
GAME( 2004, tighooka,    tighook,  sfbonus,    amcoetype1,    tighook,  ROT0,  "Amcoe", "Tiger Hook (Version 1.7)", 0)

GAME( 2004, tighookc,    tighook,  sfbonus,    amcoetype1,    tighook,  ROT0,  "Amcoe", "Tiger Hook (Version 2.1R, set 1)", 0)
GAME( 2004, tighookd,    tighook,  sfbonus,    amcoetype1,    tighookd, ROT0,  "Amcoe", "Tiger Hook (Version 2.1R, set 2)", 0)
GAME( 2004, tighookv,    tighook,  sfbonus,    amcoetype1,    tighookv, ROT0,  "Amcoe", "Tiger Hook (Version 2.1R Dual)", 0)

GAME( 2004, tighookc2,   tighook,  sfbonus,    amcoetype1,    tighook,  ROT0,  "Amcoe", "Tiger Hook (Version 2.0LT, set 1)", 0)
GAME( 2004, tighookd2,   tighook,  sfbonus,    amcoetype1,    tighookd, ROT0,  "Amcoe", "Tiger Hook (Version 2.0LT, set 2)", 0)
GAME( 2004, tighookv3,   tighook,  sfbonus,    amcoetype1,    tighookv, ROT0,  "Amcoe", "Tiger Hook (Version 2.0LT Dual)", 0)

GAME( 2004, tighookv2,   tighook,  sfbonus,    amcoetype1,    tighookv2,ROT0,  "Amcoe", "Tiger Hook (Version 2.1E Dual)", 0)

GAME( 2003, robadv,      0,        sfbonus,    amcoetype1,    robadv,   ROT0,  "Amcoe", "Robin Adventure (Version 1.5)", 0)

GAME( 2003, robadvc,     robadv,   sfbonus,    amcoetype1,    robadv,   ROT0,  "Amcoe", "Robin Adventure (Version 1.7R, set 1)", 0)
GAME( 2003, robadvd,     robadv,   sfbonus,    amcoetype1,    robadv2d, ROT0,  "Amcoe", "Robin Adventure (Version 1.7R, set 2)", 0)
GAME( 2003, robadvv2,    robadv,   sfbonus,    amcoetype1,    robadv2v1,ROT0,  "Amcoe", "Robin Adventure (Version 1.7R Dual)", 0)

GAME( 2003, robadvv1,    robadv,   sfbonus,    amcoetype1,    robadv2v4,ROT0,  "Amcoe", "Robin Adventure (Version 1.7E Dual)", 0)

GAME( 2004, robadv2,     0,        sfbonus,    amcoetype1,    robadv,   ROT0,  "Amcoe", "Robin Adventure 2 (Version 1.5SH)", 0)

GAME( 2004, robadv2a,    robadv2,  sfbonus,    amcoetype1,    robadv,   ROT0,  "Amcoe", "Robin Adventure 2 (Version 1.5)", 0)

GAME( 2004, robadv2c,    robadv2,  sfbonus,    amcoetype1,    robadv,   ROT0,  "Amcoe", "Robin Adventure 2 (Version 1.7LT, set 1)", 0)
GAME( 2004, robadv2d,    robadv2,  sfbonus,    amcoetype1,    robadv2d, ROT0,  "Amcoe", "Robin Adventure 2 (Version 1.7LT, set 2)", 0)
GAME( 2004, robadv2v3,   robadv2,  sfbonus,    amcoetype1,    robadv2v1,ROT0,  "Amcoe", "Robin Adventure 2 (Version 1.7LT Dual)", 0)

GAME( 2004, robadv2d2,   robadv2,  sfbonus,    amcoetype1,    robadv2d, ROT0,  "Amcoe", "Robin Adventure 2 (Version 1.7R, set 1)", 0)
GAME( 2004, robadv2c2,   robadv2,  sfbonus,    amcoetype1,    robadv,   ROT0,  "Amcoe", "Robin Adventure 2 (Version 1.7R, set 2)", 0)
GAME( 2004, robadv2v2,   robadv2,  sfbonus,    amcoetype1,    robadv2v1,ROT0,  "Amcoe", "Robin Adventure 2 (Version 1.7R Dual)", 0)

GAME( 2004, robadv2d3,   robadv2,  sfbonus,    amcoetype1,    robadv2d, ROT0,  "Amcoe", "Robin Adventure 2 (Version 1.7SH, set 1)", 0)
GAME( 2004, robadv2c3,   robadv2,  sfbonus,    amcoetype1,    robadv,   ROT0,  "Amcoe", "Robin Adventure 2 (Version 1.7SH, set 2)", 0)
GAME( 2004, robadv2v1,   robadv2,  sfbonus,    amcoetype1,    robadv2v1,ROT0,  "Amcoe", "Robin Adventure 2 (Version 1.7SH Dual)", 0)

GAME( 2004, robadv2v4,   robadv2,  sfbonus,    amcoetype1,    robadv2v4,ROT0,  "Amcoe", "Robin Adventure 2 (Version 1.7E Dual)", 0)

GAME( 2003, pirpok2,     0,        sfbonus,    amcoetype2,    pirpok2, ROT0,  "Amcoe", "Pirate Poker II (Version 2.0)", 0)

GAME( 2003, pirpok2b,    pirpok2,  sfbonus,    amcoetype2,    pirpok2, ROT0,  "Amcoe", "Pirate Poker II (Version 2.2R, set 1)", 0)
GAME( 2003, pirpok2d,    pirpok2,  sfbonus,    amcoetype1,    pirpok2d,ROT0,  "Amcoe", "Pirate Poker II (Version 2.2R, set 2)", 0)
GAME( 2003, pirpok2v,    pirpok2,  sfbonus,    amcoetype1,    pirpok2v,ROT0,  "Amcoe", "Pirate Poker II (Version 2.2R Dual)", 0)

GAME( 2003, pirpok2v2,   pirpok2,  sfbonus,    amcoetype1,   pirpok2v2,ROT0,  "Amcoe", "Pirate Poker II (Version 2.4E Dual)", 0)

GAME( 2003, anibonus,    0,        sfbonus,    amcoetype2,    anibonus, ROT0,  "Amcoe", "Animal Bonus (Version 1.50XT)", 0)

GAME( 2003, anibonus2,   anibonus, sfbonus,    amcoetype2,    anibonus, ROT0,  "Amcoe", "Animal Bonus (Version 1.5)", 0)

GAME( 2003, anibonus3,   anibonus, sfbonus,    amcoetype2,    anibonus3,ROT0,  "Amcoe", "Animal Bonus (Version 1.40XT, set 1)", 0)
GAME( 2003, anibonus4,   anibonus, sfbonus,    amcoetype2,    anibonus, ROT0,  "Amcoe", "Animal Bonus (Version 1.40XT, set 2)", 0)

GAME( 2003, anibonus5,   anibonus, sfbonus,    amcoetype2,    anibonus3,ROT0,  "Amcoe", "Animal Bonus (Version 1.4, set 1)", 0)
GAME( 2003, anibonus6,   anibonus, sfbonus,    amcoetype2,    anibonus, ROT0,  "Amcoe", "Animal Bonus (Version 1.4, set 2)", 0)

GAME( 2003, anibonusb,   anibonus, sfbonus,    amcoetype2,    anibonus, ROT0,  "Amcoe", "Animal Bonus (Version 1.7R, set 1)", 0)
GAME( 2003, anibonusd,   anibonus, sfbonus,    amcoetype1,    anibonusd,ROT0,  "Amcoe", "Animal Bonus (Version 1.7R, set 2)", 0)
GAME( 2003, anibonusv,   anibonus, sfbonus,    amcoetype1,    anibonusv,ROT0,  "Amcoe", "Animal Bonus (Version 1.8R Dual)", 0)

GAME( 2003, anibonusb2,  anibonus, sfbonus,    amcoetype2,    anibonus, ROT0,  "Amcoe", "Animal Bonus (Version 1.7LT, set 1)", 0)
GAME( 2003, anibonusd2,  anibonus, sfbonus,    amcoetype1,    anibonusd,ROT0,  "Amcoe", "Animal Bonus (Version 1.7LT, set 2)", 0)
GAME( 2003, anibonusv2,  anibonus, sfbonus,    amcoetype1,    anibonusv,ROT0,  "Amcoe", "Animal Bonus (Version 1.8LT Dual)", 0)

GAME( 2003, anibonusv3,  anibonus, sfbonus,    amcoetype1,   anibonusv3,ROT0,  "Amcoe", "Animal Bonus (Version 1.8E Dual)", 0)

GAME( 2000, abnudge,     0,        sfbonus,    amcoetype2,    abnudge, ROT0,  "Amcoe", "Animal Bonus Nudge (Version 1.7)", 0)
GAME( 2000, abnudgeb,    abnudge,  sfbonus,    amcoetype2,    abnudge, ROT0,  "Amcoe", "Animal Bonus Nudge (Version 2.0, set 1)", 0)
GAME( 2000, abnudged,    abnudge,  sfbonus,    amcoetype1,    abnudged,ROT0,  "Amcoe", "Animal Bonus Nudge (Version 2.0, set 2)", 0)
GAME( 2000, abnudgev,    abnudge,  sfbonus,    amcoetype1,    abnudgev,ROT0,  "Amcoe", "Animal Bonus Nudge (Version 2.1 Dual)", 0)

GAME( 2003, dblchal,     0,        sfbonus,    amcoetype2,    dblchal, ROT0,  "Amcoe", "Double Challenge (Version 1.1)", 0)

GAME( 2003, dblchalb,    dblchal,  sfbonus,    amcoetype2,    dblchal, ROT0,  "Amcoe", "Double Challenge (Version 1.5R, set 1)", 0)
GAME( 2003, dblchalc,    dblchal,  sfbonus,    amcoetype1,    dblchal, ROT0,  "Amcoe", "Double Challenge (Version 1.5R, set 2)", 0)
GAME( 2003, dblchald,    dblchal,  sfbonus,    amcoetype1,    dblchald,ROT0,  "Amcoe", "Double Challenge (Version 1.5R, set 3)", 0)
GAME( 2003, dblchalv,    dblchal,  sfbonus,    amcoetype1,    dblchalv,ROT0,  "Amcoe", "Double Challenge (Version 1.5R Dual)", 0)

GAME( 2003, anithunt,    0,        sfbonus,    amcoetype2,    anithunt, ROT0,  "Amcoe", "Animal Treasure Hunt (Version 1.7)", 0)

GAME( 2003, anithunt2,   anithunt, sfbonus,    amcoetype2,    anithunt, ROT0,  "Amcoe", "Animal Treasure Hunt (Version 1.5)", 0)

GAME( 2003, anithuntb,   anithunt, sfbonus,    amcoetype2,    anithunt, ROT0,  "Amcoe", "Animal Treasure Hunt (Version 1.9R, set 1)", 0)
GAME( 2003, anithuntd,   anithunt, sfbonus,    amcoetype1,    anithuntd,ROT0,  "Amcoe", "Animal Treasure Hunt (Version 1.9R, set 2)", 0)
GAME( 2003, anithuntv,   anithunt, sfbonus,    amcoetype1,    anithuntv,ROT0,  "Amcoe", "Animal Treasure Hunt (Version 1.9R Dual)", 0)

GAME( 2002, sfruitb2,    sfruitb,  sfbonus,    amcoetype2,    sfruitb, ROT0,  "Amcoe", "Super Fruit Bonus (Version 2.0)", 0)
GAME( 2002, sfruitb,     0,        sfbonus,    amcoetype2,    sfruitb, ROT0,  "Amcoe", "Super Fruit Bonus (Version 2.0B)", 0) // 'high bonus version'

GAME( 2002, sfruitbb,    sfruitb,  sfbonus,    amcoetype2,    sfruitb, ROT0,  "Amcoe", "Super Fruit Bonus (Version 2.0LT, set 1)", 0)
GAME( 2002, sfruitbd,    sfruitb,  sfbonus,    amcoetype1,    sfruitbd,ROT0,  "Amcoe", "Super Fruit Bonus (Version 2.0LT, set 2)", 0)
GAME( 2002, sfruitbv5,   sfruitb,  sfbonus,    amcoetype1,    sfruitbv,ROT0,  "Amcoe", "Super Fruit Bonus (Version 2.0LT Dual)", 0)

GAME( 2002, sfruitbb2,   sfruitb,  sfbonus,    amcoetype2,    sfruitb, ROT0,  "Amcoe", "Super Fruit Bonus (Version 2.2B, set 1)", 0) // 'high bonus version'
GAME( 2002, sfruitbd2,   sfruitb,  sfbonus,    amcoetype1,    sfruitbd,ROT0,  "Amcoe", "Super Fruit Bonus (Version 2.2B, set 2)", 0) // 'high bonus version'
GAME( 2002, sfruitbv4,   sfruitb,  sfbonus,    amcoetype1,    sfruitbv,ROT0,  "Amcoe", "Super Fruit Bonus (Version 2.2B Dual)", 0) // 'high bonus version'

GAME( 2002, sfruitbb3,   sfruitb,  sfbonus,    amcoetype2,    sfruitb, ROT0,  "Amcoe", "Super Fruit Bonus (Version 2.5R, set 1)", 0)
GAME( 2002, sfruitbd3,   sfruitb,  sfbonus,    amcoetype1,    sfruitbd,ROT0,  "Amcoe", "Super Fruit Bonus (Version 2.5R, set 2)", 0)
GAME( 2002, sfruitbv,    sfruitb,  sfbonus,    amcoetype1,    sfruitbv,ROT0,  "Amcoe", "Super Fruit Bonus (Version 2.5R Dual)", 0)

GAME( 2002, sfruitbv2,    sfruitb,  sfbonus,    amcoetype1,    sfruitbv2, ROT0,  "Amcoe", "Super Fruit Bonus (Version 2.5E Dual)", 0)
GAME( 2002, sfruitbv3,    sfruitb,  sfbonus,    amcoetype1,    sfruitbv2, ROT0,  "Amcoe", "Super Fruit Bonus (Version 2.2EB Dual)", 0) // 'high bonus version'

GAME( 2002, sfruitb3,    sfruitb,  sfbonus,    amcoetype2,    sfruitb, ROT0,  "Amcoe", "Super Fruit Bonus (Version 1.80XT)", 0)

GAME( 2004, fb2gen,      0,        sfbonus,    amcoetype1,    fb2gen, ROT0,  "Amcoe", "Fruit Bonus 2nd Generation (Version 1.6XT)", 0)

GAME( 2004, fb2gena,     fb2gen,   sfbonus,    amcoetype1,    fb2gen, ROT0,  "Amcoe", "Fruit Bonus 2nd Generation (Version 1.5)", 0)

GAME( 2004, fb2genc,     fb2gen,   sfbonus,    amcoetype1,    fb2gen, ROT0,  "Amcoe", "Fruit Bonus 2nd Generation (Version 1.8R, set 1)", 0)
GAME( 2004, fb2gend,     fb2gen,   sfbonus,    amcoetype1,    fb2gend,ROT0,  "Amcoe", "Fruit Bonus 2nd Generation (Version 1.8R, set 2)", 0)
GAME( 2004, fb2genv,     fb2gen,   sfbonus,    amcoetype1,    fb2genv,ROT0,  "Amcoe", "Fruit Bonus 2nd Generation (Version 1.8R Dual)", 0)

GAME( 2004, fb2genc2,    fb2gen,   sfbonus,    amcoetype1,    fb2gen, ROT0,  "Amcoe", "Fruit Bonus 2nd Generation (Version 1.8LT, set 1)", 0)
GAME( 2004, fb2gend2,    fb2gen,   sfbonus,    amcoetype1,    fb2gend,ROT0,  "Amcoe", "Fruit Bonus 2nd Generation (Version 1.8LT, set 2)", 0)
GAME( 2004, fb2genv2,    fb2gen,   sfbonus,    amcoetype1,    fb2genv,ROT0,  "Amcoe", "Fruit Bonus 2nd Generation (Version 1.8LT Dual)", 0)

GAME( 2004, fb2genv3,    fb2gen,   sfbonus,    amcoetype1,   fb2genv3,ROT0,  "Amcoe", "Fruit Bonus 2nd Generation (Version 1.8E Dual)", 0)


GAME( 2004, fb2nd,       0,       sfbonus,    amcoetype1,    fb2nd, ROT0,  "Amcoe", "Fruit Bonus 2nd Edition (Version 1.5)", 0)

GAME( 2004, fb2ndc,      fb2nd,   sfbonus,    amcoetype1,    fb2nd, ROT0,  "Amcoe", "Fruit Bonus 2nd Edition (Version 1.8R, set 1)", 0)
GAME( 2004, fb2ndd,      fb2nd,   sfbonus,    amcoetype1,    fb2ndd,ROT0,  "Amcoe", "Fruit Bonus 2nd Edition (Version 1.8R, set 2)", 0)
GAME( 2004, fb2ndv,      fb2nd,   sfbonus,    amcoetype1,    fb2ndv,ROT0,  "Amcoe", "Fruit Bonus 2nd Edition (Version 1.8R Dual)", 0)

GAME( 2004, fb2ndc2,     fb2nd,   sfbonus,    amcoetype1,    fb2nd, ROT0,  "Amcoe", "Fruit Bonus 2nd Edition (Version 1.8LT, set 1)", 0)
GAME( 2004, fb2ndd2,     fb2nd,   sfbonus,    amcoetype1,    fb2ndd,ROT0,  "Amcoe", "Fruit Bonus 2nd Edition (Version 1.8LT, set 2)", 0)
GAME( 2004, fb2ndv2,     fb2nd,   sfbonus,    amcoetype1,    fb2ndv,ROT0,  "Amcoe", "Fruit Bonus 2nd Edition (Version 1.8LT Dual)", 0)



GAME( 2000, fb4,         0,        sfbonus,    amcoetype2,    fb4, ROT0,  "Amcoe", "Fruit Bonus 4 (Version 1.3XT)", 0)
GAME( 2000, fb4a,        fb4,      sfbonus,    amcoetype2,    fb4, ROT0,  "Amcoe", "Fruit Bonus 4 (Version 1.2)", 0)

GAME( 2000, fb4b,        fb4,      sfbonus,    amcoetype2,    fb4, ROT0,  "Amcoe", "Fruit Bonus 4 (Version 1.5LT, set 1", 0)
GAME( 2000, fb4c2,       fb4,      sfbonus,    amcoetype1,    fb4, ROT0,  "Amcoe", "Fruit Bonus 4 (Version 1.5LT, set 2)", 0)
GAME( 2000, fb4d2,       fb4,      sfbonus,    amcoetype1,    fb4d,ROT0,  "Amcoe", "Fruit Bonus 4 (Version 1.5LT, set 3)", 0)
GAME( 2000, fb4v2,       fb4,      sfbonus,    amcoetype1,    fb4v,ROT0,  "Amcoe", "Fruit Bonus 4 (Version 1.5LT Dual)", 0)

GAME( 2000, fb4b2,       fb4,      sfbonus,    amcoetype2,    fb4, ROT0,  "Amcoe", "Fruit Bonus 4 (Version 1.5R, set 1)", 0)
GAME( 2000, fb4c,        fb4,      sfbonus,    amcoetype1,    fb4, ROT0,  "Amcoe", "Fruit Bonus 4 (Version 1.5R, set 2)", 0)
GAME( 2000, fb4d,        fb4,      sfbonus,    amcoetype1,    fb4d,ROT0,  "Amcoe", "Fruit Bonus 4 (Version 1.5R, set 3)", 0)
GAME( 2000, fb4v,        fb4,      sfbonus,    amcoetype1,    fb4v,ROT0,  "Amcoe", "Fruit Bonus 4 (Version 1.5R Dual)", 0)

GAME( 2000, fb4v3,       fb4,      sfbonus,    amcoetype1,    fb4v3, ROT0, "Amcoe", "Fruit Bonus 4 (Version 1.5E Dual) (with 2005 title)", 0) // the export version has '2005' title, but is considered the same game as fb4 and labeled as such


GAME( 1999, act2000,     0,        sfbonus,    amcoetype2,    act2000, ROT0,  "Amcoe", "Action 2000 (Version 1.2)", 0)
GAME( 1999, act2000a,    act2000,  sfbonus,    amcoetype2,    act2000, ROT0,  "Amcoe", "Action 2000 (Version 3.3)", 0)
GAME( 1999, act2000a2,   act2000,  sfbonus,    amcoetype2,    act2000, ROT0,  "Amcoe", "Action 2000 (Version 3.10XT)", 0)
GAME( 1999, act2000v,    act2000,  sfbonus,    amcoetype1,    act2000v, ROT0,  "Amcoe", "Action 2000 (Version 3.5R Dual)", 0)
GAME( 1999, act2000v2,   act2000,  sfbonus,    amcoetype1,    act2000v2, ROT0,  "Amcoe", "Action 2000 (Version 3.5E Dual)", 0)
GAME( 1999, act2000v3,   act2000,  sfbonus,    amcoetype1,    act2000v3, ROT0,  "Amcoe", "Action 2000 (Version 3.30XT Dual)", 0)
GAME( 1999, act2000d,    act2000,  sfbonus,    amcoetype1,    act2000d, ROT0,  "Amcoe", "Action 2000 (Version 3.5R, set 1)", 0)
GAME( 1999, act2000d2,   act2000,  sfbonus,    amcoetype1,    act2000d, ROT0,  "Amcoe", "Action 2000 (Version 3.30XT, set 1)", 0)
GAME( 1999, act2000b,    act2000,  sfbonus,    amcoetype2,    act2000, ROT0,  "Amcoe", "Action 2000 (Version 3.5R, set 2)", 0)
GAME( 1999, act2000b2,   act2000,  sfbonus,    amcoetype2,    act2000, ROT0,  "Amcoe", "Action 2000 (Version 3.30XT, set 2)", 0)

GAME( 2000, ch2000,      0,        sfbonus,    amcoetype2,    ch2000, ROT0,  "Amcoe", "Fruit Bonus 2000 / New Cherry 2000 (Version 3.9XT)", 0)
GAME( 2000, ch2000x,     ch2000,   sfbonus,    amcoetype2,    ch2000, ROT0,  "Amcoe", "Fruit Bonus 2000 / New Cherry 2000 (Version 3.9D)", 0)
GAME( 2000, ch2000y,     ch2000,   sfbonus,    amcoetype2,    ch2000, ROT0,  "Amcoe", "Fruit Bonus 2000 / New Cherry 2000 (Version 3.9)", 0)

GAME( 2000, ch2000b2,    ch2000,   sfbonus,    amcoetype2,    ch2000, ROT0,  "Amcoe", "Fruit Bonus 2000 / New Cherry 2000 (Version 4.1LT, set 1)", 0)
GAME( 2000, ch2000c,     ch2000,   sfbonus,    amcoetype1,    ch2000c, ROT0,  "Amcoe", "Fruit Bonus 2000 / New Cherry 2000 (Version 4.1LT, set 2)", 0)
GAME( 2000, ch2000d,     ch2000,   sfbonus,    amcoetype1,    ch2000d, ROT0,  "Amcoe", "Fruit Bonus 2000 / New Cherry 2000 (Version 4.1LT, set 3)", 0)
GAME( 2000, ch2000v3,    ch2000,   sfbonus,    amcoetype1,    ch2000v3,ROT0,  "Amcoe", "Fruit Bonus 2000 / New Cherry 2000 (Version 4.1LT Dual)", 0)

GAME( 2000, ch2000b,     ch2000,   sfbonus,    amcoetype2,    ch2000, ROT0,  "Amcoe", "Fruit Bonus 2000 / New Cherry 2000 (Version 4.4R, set 1)", 0)
GAME( 2000, ch2000c2,    ch2000,   sfbonus,    amcoetype1,    ch2000c,ROT0,  "Amcoe", "Fruit Bonus 2000 / New Cherry 2000 (Version 4.4R, set 2)", 0)
GAME( 2000, ch2000d2,    ch2000,   sfbonus,    amcoetype1,    ch2000d, ROT0,  "Amcoe", "Fruit Bonus 2000 / New Cherry 2000 (Version 4.4R, set 3)", 0)
GAME( 2000, ch2000v,     ch2000,   sfbonus,    amcoetype1,    ch2000v, ROT0,  "Amcoe", "Fruit Bonus 2000 / New Cherry 2000 (Version 4.4R Dual)", 0)
GAME( 2000, ch2000v2,    ch2000,   sfbonus,    amcoetype1,    ch2000v2,ROT0,  "Amcoe", "Fruit Bonus 2000 / New Cherry 2000 (Version 4.4E Dual)", 0)

GAME( 2001, pir2001,     0,        sfbonus,    amcoetype2,    pir2001, ROT0,  "Amcoe", "Pirate 2001 (Version 2.3N)", 0)
GAME( 2001, pir2001a,    pir2001,  sfbonus,    amcoetype2,    pir2001, ROT0,  "Amcoe", "Pirate 2001 (Version 2.3)", 0)
GAME( 2001, pir2001b,    pir2001,  sfbonus,    amcoetype2,    pir2001, ROT0,  "Amcoe", "Pirate 2001 (Version 2.20XT)", 0)

GAME( 2001, pir2001b2,   pir2001,  sfbonus,    amcoetype2,    pir2001, ROT0,  "Amcoe", "Pirate 2001 (Version 2.40XT, set 1)", 0)
GAME( 2001, pir2001d2,   pir2001,  sfbonus,    amcoetype1,    pir2001d,ROT0,  "Amcoe", "Pirate 2001 (Version 2.40XT, set 2)", 0)
GAME( 2001, pir2001v3,   pir2001,  sfbonus,    amcoetype1,    pir2001v,ROT0,  "Amcoe", "Pirate 2001 (Version 2.40XT Dual)", 0)

GAME( 2001, pir2001b3,   pir2001,  sfbonus,    amcoetype2,    pir2001, ROT0,  "Amcoe", "Pirate 2001 (Version 2.5R, set 1)", 0)
GAME( 2001, pir2001d,    pir2001,  sfbonus,    amcoetype1,    pir2001d,ROT0,  "Amcoe", "Pirate 2001 (Version 2.5R, set 2)", 0)
GAME( 2001, pir2001v,    pir2001,  sfbonus,    amcoetype1,    pir2001v,ROT0,  "Amcoe", "Pirate 2001 (Version 2.5R Dual)", 0)

GAME( 2001, pir2001v2,   pir2001,  sfbonus,    amcoetype1,    pir2001v2,ROT0, "Amcoe", "Pirate 2001 (Version 2.5E Dual)", 0)

GAME( 2002, pir2002,     0,        sfbonus,    amcoetype2,    pir2002, ROT0,  "Amcoe", "Pirate 2002 (Version 1.8N)", 0)
GAME( 2002, pir2002a,    pir2002,  sfbonus,    amcoetype2,    pir2002, ROT0,  "Amcoe", "Pirate 2002 (Version 1.8)", 0)
GAME( 2002, pir2002b,    pir2002,  sfbonus,    amcoetype2,    pir2002, ROT0,  "Amcoe", "Pirate 2002 (Version 1.70XT)", 0)

GAME( 2002, pir2002b2,   pir2002,  sfbonus,    amcoetype2,    pir2002, ROT0,  "Amcoe", "Pirate 2002 (Version 1.90XT, set 1)", 0)
GAME( 2002, pir2002d2,   pir2002,  sfbonus,    amcoetype1,    pir2002d,ROT0,  "Amcoe", "Pirate 2002 (Version 1.90XT, set 2)", 0)
GAME( 2002, pir2002v3,   pir2002,  sfbonus,    amcoetype1,    pir2002v, ROT0, "Amcoe", "Pirate 2002 (Version 1.90XT Dual)", 0)

GAME( 2002, pir2002b3,   pir2002,  sfbonus,    amcoetype2,    pir2002, ROT0,  "Amcoe", "Pirate 2002 (Version 2.0R, set 1)", 0)
GAME( 2002, pir2002d,    pir2002,  sfbonus,    amcoetype1,    pir2002d, ROT0, "Amcoe", "Pirate 2002 (Version 2.0R, set 2)", 0)
GAME( 2002, pir2002v,    pir2002,  sfbonus,    amcoetype1,    pir2002v,ROT0,  "Amcoe", "Pirate 2002 (Version 2.0R Dual)", 0)

GAME( 2002, pir2002v2,   pir2002,  sfbonus,    amcoetype1,    pir2002v2,ROT0, "Amcoe", "Pirate 2002 (Version 2.0E Dual)", 0)

GAME( 200?, classice,    0,        sfbonus,    amcoetype1,    classice, ROT0,  "Amcoe", "Classic Edition (Version 1.6LT, set 1)", 0)
GAME( 200?, classiced2,  classice, sfbonus,    amcoetype1,    classiced,ROT0,  "Amcoe", "Classic Edition (Version 1.6LT, set 2)", 0)
GAME( 200?, classicev2,  classice, sfbonus,    amcoetype1,    classicev,ROT0,  "Amcoe", "Classic Edition (Version 1.6LT Dual)", 0)

GAME( 200?, classicea,   classice, sfbonus,    amcoetype1,    classice, ROT0,  "Amcoe", "Classic Edition (Version 1.6R, set 1)", 0)
GAME( 200?, classiced,   classice, sfbonus,    amcoetype1,    classiced,ROT0,  "Amcoe", "Classic Edition (Version 1.6R, set 2)", 0)
GAME( 200?, classicev,   classice, sfbonus,    amcoetype1,    classicev,ROT0,  "Amcoe", "Classic Edition (Version 1.6R Dual)", 0)

GAME( 200?, classiced3,  classice, sfbonus,    amcoetype1,    classiced3,ROT0,  "Amcoe", "Classic Edition (Version 1.6E)", 0)
GAME( 200?, classicev3,  classice, sfbonus,    amcoetype1,    classicev3,ROT0,  "Amcoe", "Classic Edition (Version 1.6E Dual)", 0)

GAME( 200?, seawld,      0,        sfbonus,    amcoetype1,    seawld, ROT0,  "Amcoe", "Sea World (Version 1.6R CGA)", 0)
GAME( 200?, seawlda,     seawld,   sfbonus,    amcoetype1,    seawlda, ROT0,  "Amcoe", "Sea World (Version 1.6E Dual)", 0)

GAME( 200?, moneymacd,   moneymac, sfbonus,    amcoetype1,    moneymacd,ROT0,  "Amcoe", "Money Machine (Version 1.7R)", 0)
GAME( 200?, moneymac,    0,        sfbonus,    amcoetype1,    moneymac, ROT0,  "Amcoe", "Money Machine (Version 1.7R Dual)", 0)

GAME( 200?, moneymacd2,  moneymac, sfbonus,    amcoetype1,    moneymacd, ROT0, "Amcoe", "Money Machine (Version 1.7LT)", 0)
GAME( 200?, moneymacv2,  moneymac, sfbonus,    amcoetype1,    moneymac, ROT0,  "Amcoe", "Money Machine (Version 1.7LT Dual", 0)

GAME( 200?, moneymacv,   moneymac, sfbonus,    amcoetype1,    moneymacv, ROT0,  "Amcoe", "Money Machine (Version 1.7E Dual)", 0)

GAME( 200?, atworld,     0,        sfbonus,    amcoetype1,    atworld, ROT0,  "Amcoe", "Around The World (Version 1.3R CGA)", 0)
GAME( 200?, atworlda,    atworld,  sfbonus,    amcoetype1,    atworlda, ROT0, "Amcoe", "Around The World (Version 1.3E CGA)", 0)

GAME( 2000, fb5,         0,        sfbonus,    amcoetype2,    fb5, ROT0,  "Amcoe", "Fruit Bonus 5 (Version 1.5SH, set 1)", 0)
GAME( 2000, fb5c,        fb5,      sfbonus,    amcoetype1,    fb5, ROT0,  "Amcoe", "Fruit Bonus 5 (Version 1.5SH, set 2)", 0)
GAME( 2000, fb5d,        fb5,      sfbonus,    amcoetype1,    fb5d, ROT0,  "Amcoe", "Fruit Bonus 5 (Version 1.5SH, set 3)", 0)
GAME( 2000, fb5v,        fb5,      sfbonus,    amcoetype1,    fb5v, ROT0,  "Amcoe", "Fruit Bonus 5 (Version 1.5SH Dual)", 0)

GAME( 2000, fb6,         0,        sfbonus,    amcoetype1,    fb6, ROT0,  "Amcoe", "Fruit Bonus 6 (Version 1.7E CGA)", 0)

GAME( 2000, fb6v3,       fb6,      sfbonus,    amcoetype1,    fb6v3, ROT0,  "Amcoe", "Fruit Bonus 6 (Version 1.7E Dual)", 0)

GAME( 2000, fb6d2,       fb6,      sfbonus,    amcoetype1,    fb6d, ROT0,  "Amcoe", "Fruit Bonus 6 (Version 1.7R CGA, set 1)", 0)
GAME( 2000, fb6s,        fb6,      sfbonus,    amcoetype1,    fb6s, ROT0,  "Amcoe", "Fruit Bonus 6 (Version 1.7R CGA, set 2)", GAME_NOT_WORKING) // no reel gfx
GAME( 2000, fb6v,        fb6,      sfbonus,    amcoetype1,    fb6v, ROT0,  "Amcoe", "Fruit Bonus 6 (Version 1.7R Dual)", 0)

GAME( 2000, fb6d,        fb6,      sfbonus,    amcoetype1,    fb6d, ROT0,  "Amcoe", "Fruit Bonus 6 (Version 1.7LT CGA, set 1)", 0)
GAME( 2000, fb6s2,       fb6,      sfbonus,    amcoetype1,    fb6s, ROT0,  "Amcoe", "Fruit Bonus 6 (Version 1.7LT CGA, set 2)", GAME_NOT_WORKING) // no reel gfx
GAME( 2000, fb6v2,       fb6,      sfbonus,    amcoetype1,    fb6v, ROT0,  "Amcoe", "Fruit Bonus 6 (Version 1.7LT Dual)", 0)

GAME( 2000, fb6se,       0,        sfbonus,    amcoetype1,    fb6d, ROT0,  "Amcoe", "Fruit Bonus 6 Special Edition (Version 1.4R CGA)", 0)
GAME( 2000, fb6sev,      fb6se,    sfbonus,    amcoetype1,    fb6v, ROT0,  "Amcoe", "Fruit Bonus 6 Special Edition (Version 1.4R Dual)", 0)

GAME( 2000, fb6sed,      fb6se,    sfbonus,    amcoetype1,    fb6d, ROT0,  "Amcoe", "Fruit Bonus 6 Special Edition (Version 1.4LT CGA)", 0)
GAME( 2000, fb6sev2,     fb6se,    sfbonus,    amcoetype1,    fb6v, ROT0,  "Amcoe", "Fruit Bonus 6 Special Edition (Version 1.4LT Dual)", 0)

GAME( 2000, fb6sed2,     fb6se,    sfbonus,    amcoetype1,    fb6, ROT0,  "Amcoe", "Fruit Bonus 6 Special Edition (Version 1.4E CGA)", 0)
GAME( 2000, fb6sev3,     fb6se,    sfbonus,    amcoetype1,    fb6v3, ROT0,  "Amcoe", "Fruit Bonus 6 Special Edition (Version 1.4E Dual)", 0)

// no gfx / sound roms
GAME( 2000, version4,    0,        sfbonus,    amcoetype1,    version4, ROT0,  "Amcoe", "Version 4 (Version 4.2R)", GAME_NOT_WORKING)
// no gfx / sound roms
GAME( 2000, bugfever,    0,        sfbonus,    amcoetype1,    bugfever, ROT0,  "Amcoe", "Bug Fever (set 1)", GAME_NOT_WORKING)
GAME( 2000, bugfeverd,   bugfever, sfbonus,    amcoetype1,    bugfeverd, ROT0,  "Amcoe", "Bug Fever (set 2)", GAME_NOT_WORKING)
GAME( 2000, bugfeverv,   bugfever, sfbonus,    amcoetype1,    bugfeverv, ROT0,  "Amcoe", "Bug Fever (set 3)", GAME_NOT_WORKING)
GAME( 2000, bugfeverv2,  bugfever, sfbonus,    amcoetype1,    bugfeverv2, ROT0,  "Amcoe", "Bug Fever (set 4)", GAME_NOT_WORKING)
// no gfx / sound roms
GAME( 2000, dvisland,    0,        sfbonus,    amcoetype1,    dvisland, ROT0,  "Amcoe", "Devil's Island", GAME_NOT_WORKING)
// no gfx / sound roms
GAME( 2000, funriver,    0,        sfbonus,    amcoetype1,    funriver, ROT0,  "Amcoe", "Fun River (set 1)", GAME_NOT_WORKING)
GAME( 2000, funriverv,   funriver, sfbonus,    amcoetype1,    funriverv, ROT0,  "Amcoe", "Fun River (set 2)", GAME_NOT_WORKING)
// ?? what is this
GAME( 2000, amclink,    0,        sfbonus,    amcoetype1,    sfbonus_common, ROT0,  "Amcoe", "Amcoe Link Control Box (Version 2.2)", GAME_NOT_WORKING)