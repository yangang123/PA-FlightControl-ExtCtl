/*
 * nav_block_mission.c
 *
 *  Created on: Jul 9, 2018
 *      Author: lidq
 */

#include "nav_block_mission.h"

#define WP_MAX (100)

static ext_vehicle_sp_s _sp = { 0 };

static airline_s _airline = { 0 };

static int _wps_grp = 0;
static int _wps_ind = 0;

static waypoint_s *_waypoints[2] = { NULL, NULL };
static int _waypoint_cnt[2] = { 0, 0 };
static int _load_airline_id = 0;
static int _load_waypoint_index = 0;

static hrt_abstime _loiter_time = 0;

void nav_mission_on_init(void)
{
	_waypoint_cnt[0] = 0;
	_waypoint_cnt[1] = 0;

	_load_airline_id = 0;
	_load_waypoint_index = 0;

	_loiter_time = 0;

	int ret = airline_get_airline(_load_airline_id, &_airline);

	if (ret < 0)
	{
		printf("[nav] get airline error.\n");
		return;
	}

	_waypoints[0] = malloc(sizeof(waypoint_s) * WP_MAX);
	_waypoints[1] = malloc(sizeof(waypoint_s) * WP_MAX);

	if (_waypoints[0] == NULL)
	{
		printf("[nav] malloc _waypoints[0] error.\n");
		return;
	}

	if (_waypoints[1] == NULL)
	{
		printf("[nav] malloc _waypoints[1] error.\n");
		return;
	}

	nav_mission_load_seque(0);
	nav_mission_load_seque(1);

	_wps_grp = 0;
	_wps_ind = 0;
}

void nav_mission_on_desc(void)
{

}

void nav_mission_on_inactive(void)
{

}

void nav_mission_on_activation(void)
{
	if (nav_mission_is_finished())
	{
		return;
	}

	nav_mission_set_yawpoint_to_sp();
}

void nav_mission_on_active(void)
{
	if (nav_mission_is_finished())
	{
		return;
	}

	ext_vehicle_pos_s *pos = navigator_get_curr_pos();

	int acc_opt = 0;
	if (_waypoints[_wps_grp][_wps_ind].accept_opt & WP_ACCEPT_OPT_XY != 0)
	{
		if (!nav_block_is_reached_xy(_waypoints[_wps_grp][_wps_ind].x, _waypoints[_wps_grp][_wps_ind].y, pos->x, pos->y))
		{
			acc_opt++;
		}
	}

	if (_waypoints[_wps_grp][_wps_ind].accept_opt & WP_ACCEPT_OPT_Z != 0)
	{
		if (!nav_block_is_reached_z(_waypoints[_wps_grp][_wps_ind].z, pos->z))
		{
			acc_opt++;
		}
	}

	if (_waypoints[_wps_grp][_wps_ind].accept_opt & WP_ACCEPT_OPT_YAW != 0)
	{
		if (!nav_block_is_reached_yaw(_waypoints[_wps_grp][_wps_ind].yaw, pos->yaw))
		{
			acc_opt++;
		}
	}

	if (acc_opt != 0)
	{
		return;
	}

	if (_waypoints[_wps_grp][_wps_ind].accept_opt == 0 || _waypoints[_wps_grp][_wps_ind].loiter_secs == 0)
	{
		_wps_ind++;
		if (_wps_ind >= _waypoint_cnt[_wps_grp])
		{
			_wps_ind = 0;
			nav_mission_load_seque(_wps_grp);
			_wps_grp = _wps_grp ? 0 : 1;
		}

		nav_mission_set_yawpoint_to_sp();

		return;
	}

	if (_waypoints[_wps_grp][_wps_ind].loiter_secs > 0)
	{
		hrt_abstime now = hrt_absolute_time();
		if (_loiter_time > 0 && _loiter_time <= now)
		{
			_loiter_time = 0;

			_wps_ind++;
			if (_wps_ind >= _waypoint_cnt[_wps_grp])
			{
				_wps_ind = 0;
				nav_mission_load_seque(_wps_grp);
				_wps_grp = _wps_grp ? 0 : 1;
			}

			nav_mission_set_yawpoint_to_sp();

			return;
		}

		if (_loiter_time < now)
		{
			_loiter_time = now + _waypoints[_wps_grp][_wps_ind].loiter_secs * 1000000;
			return;
		}
	}
}

bool nav_mission_is_finished(void)
{
	if (_wps_ind >= _waypoint_cnt[_wps_grp] && _waypoint_cnt[(_wps_grp + 1) % 2] == 0)
	{
		_wps_ind = 0;
		_waypoint_cnt[_wps_grp] = 0;
		return true;
	}

	if (_waypoint_cnt[0] == 0 && _waypoint_cnt[1] == 0)
	{
		return true;
	}
	return false;
}

int nav_mission_load_seque(int grp)
{
	_waypoint_cnt[grp] = 0;

	int ret = airline_get_airline(_load_airline_id, &_airline);
	if (ret < 0)
	{
		printf("[nav] get airline finished.\n");
		return -1;
	}

	for (; _load_airline_id < AIRLINE_MAXNUM;)
	{
		int cnt = airline_get_waypoint(&_airline, _load_waypoint_index, WP_MAX, &_waypoints[grp][_waypoint_cnt[grp]]);
		if (cnt <= 0)
		{
			printf("[nav] get waypoint error.\n");
			break;
		}
		_waypoint_cnt[grp] += cnt;

		if (cnt < WP_MAX)
		{
			_load_waypoint_index = 0;
			_load_airline_id++;

			break;
		}

		_load_waypoint_index += cnt;
		break;
	}

	return 0;
}

int nav_mission_set_yawpoint_to_sp(void)
{
	if (_wps_ind >= _waypoint_cnt[_wps_grp])
	{
		return -1;
	}

	nav_block_set_accept_params(_waypoints[_wps_grp][_wps_ind].accept_radius_xy, _waypoints[_wps_grp][_wps_ind].accept_radius_z, _waypoints[_wps_grp][_wps_ind].accept_yaw);
	if (_waypoints[_wps_grp][_wps_ind].is_local_sp)
	{
		_sp.sp_yaw = _waypoints[_wps_grp][_wps_ind].yaw;

		_sp.sp_x = _waypoints[_wps_grp][_wps_ind].x;
		_sp.sp_y = _waypoints[_wps_grp][_wps_ind].y;
		_sp.sp_z = _waypoints[_wps_grp][_wps_ind].z;
	}

	_sp.run_pos_control = true;
	_sp.run_alt_control = true;
	_sp.run_yaw_control = true;

	//printf("[nav mis] set sp %f %f %f\n", _sp.sp_x, _sp.sp_y, _sp.sp_z);

	navigator_set_sp(&_sp);

	return 0;
}
