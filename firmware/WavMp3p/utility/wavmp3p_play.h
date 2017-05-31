/*
 * wavmp3p_play.h - wavmp3p library
 * Copyright (c) 2015 Mitsuhiro Matsuura.  All right reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define f_open(p) wavmp3p_open(p)
#define f_read(p1, p2, p3) wavmp3p_read(p1, p2, p3)
#define f_close() wavmp3p_close()

#define FR_OK 0
typedef int FRESULT;
typedef unsigned int UINT;

#ifdef __cplusplus
extern "C" {
#endif

	FRESULT wavmp3p_open(const char* filename);
	FRESULT wavmp3p_read(void* buff, unsigned int btr, unsigned int* br);
	FRESULT wavmp3p_close(void);

#ifdef __cplusplus
};
#endif
