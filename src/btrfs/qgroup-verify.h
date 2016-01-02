/*
 * Copyright (C) 2014 SUSE.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License v2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _BTRFS_QGROUP_VERIFY_H
#define _BTRFS_QGROUP_VERIFY_H

int qgroup_verify_all(struct btrfs_fs_info *info);
void print_qgroup_report(int all);

int print_extent_state(struct btrfs_fs_info *info, u64 subvol);

#endif	/* _BTRFS_QGROUP_VERIFY_H */
