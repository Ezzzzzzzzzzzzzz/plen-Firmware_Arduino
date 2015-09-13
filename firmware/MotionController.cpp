﻿/*
	Copyright (c) 2015.
	- Kazuyuki TAKASE - https://github.com/Guvalif
	- PLEN Project Company Ltd. - http://plen.jp

	This software is released under the MIT License.
	(See also : http://opensource.org/licenses/mit-license.php)
*/


// Arduinoライブラリ関連
#include "Arduino.h"

// 独自ライブラリ関連
#include "System.h"
#include "ExternalEEPROM.h"
#include "MotionController.h"


// マクロの定義
#define _DEBUG false


// ファイル内グローバルインスタンスの定義
namespace {
	PLEN2::System         system;
	PLEN2::ExternalEEPROM exteeprom;


	inline static const int PRECISION() { return 16; }
	
	inline long getFixedPoint(int value)
	{
		return ((long)value << PRECISION());
	}

	inline int getUnfixedPoint(long value)
	{
		return (value >> PRECISION());
	}

	template<typename T>
	int SIZE_SUP()
	{
		return (sizeof(T) % PLEN2::ExternalEEPROM::SLOT_SIZE());
	}

	template<typename T>
	int SLOTNUM()
	{
		const int slot_count = sizeof(T) / PLEN2::ExternalEEPROM::SLOT_SIZE();

		int slot_num = 0;
		slot_num += slot_count;
		slot_num += (SIZE_SUP<T>()? 1 : 0);

		return slot_num;
	}

	int SLOTNUM_MOTION_FULL()
	{
		int slot_num = 0;
		slot_num += SLOTNUM<PLEN2::MotionController::Header>();
		slot_num += SLOTNUM<PLEN2::MotionController::Frame>() * PLEN2::MotionController::Header::FRAMELENGTH_MAX();

		return slot_num;
	}
}


PLEN2::MotionController::MotionController(JointController& joint_ctrl)
{
	_p_joint_ctrl = &joint_ctrl;

	_playing = false;
	_p_frame_now  = _buffer;
	_p_frame_next = _buffer + 1;

	for (int joint_id = 0; joint_id < JointController::SUM(); joint_id++)
	{
		_p_frame_now->joint_angle[joint_id] = 0;
	}
}


bool PLEN2::MotionController::setHeader(const Header* p_header)
{
	#if _DEBUG
		system.outputSerial().println(F("=== in fuction : MotionController::setHeader()"));
	#endif

	if (p_header->slot >= Header::SLOT_END())
	{
		#if _DEBUG
			system.outputSerial().print(F(">>> bad argment : p_header->slot = "));
			system.outputSerial().println((int)p_header->slot);
		#endif

		return false;
	}

	if (   (p_header->frame_length > Header::FRAMELENGTH_MAX())
		|| (p_header->frame_length < Header::FRAMELENGTH_MIN()) )
	{
		#if _DEBUG
			system.outputSerial().print(F(">>> bad argment : p_header->frame_length = "));
			system.outputSerial().println((int)p_header->frame_length);
		#endif

		return false;
	}

	int write_count = SLOTNUM<Header>();
	int write_size_sup = (SIZE_SUP<Header>()? SIZE_SUP<Header>() : ExternalEEPROM::SLOT_SIZE());

	const char* filler = (const char*)p_header;

	for (int count = 0; count < (write_count - 1); count++)
	{
		int ret = exteeprom.writeSlot(
			(int)p_header->slot * SLOTNUM_MOTION_FULL() + count,
			filler + ExternalEEPROM::SLOT_SIZE() * count,
			ExternalEEPROM::SLOT_SIZE()
		);

		if (ret != 0)
		{
			#if _DEBUG
				system.outputSerial().print(F(">>> failed : ret["));
				system.outputSerial().print(count);
				system.outputSerial().print(F("] = "));
				system.outputSerial().println(ret);
			#endif

			return false;
		}
	}

	int ret = exteeprom.writeSlot(
		(int)p_header->slot * SLOTNUM_MOTION_FULL() + (write_count - 1),
		filler + ExternalEEPROM::SLOT_SIZE() * (write_count - 1),
		write_size_sup
	);

	if (ret != 0)
	{
		#if _DEBUG
			system.outputSerial().print(F(">>> failed : ret["));
			system.outputSerial().print(write_count - 1);
			system.outputSerial().print(F("] = "));
			system.outputSerial().println(ret);
		#endif

		return false;
	}

	return true;
}


bool PLEN2::MotionController::getHeader(Header* p_header)
{
	#if _DEBUG
		system.outputSerial().println(F("=== in fuction : MotionController::getHeader()"));
	#endif
	
	if (p_header->slot >= Header::SLOT_END())
	{
		#if _DEBUG
			system.outputSerial().print(F(">>> bad argment : p_header->slot = "));
			system.outputSerial().println((int)p_header->slot);
		#endif

		return false;
	}

	int read_count = SLOTNUM<Header>();
	int read_size_sup = (SIZE_SUP<Header>()? SIZE_SUP<Header>() : ExternalEEPROM::SLOT_SIZE());

	char* filler = (char*)p_header;

	for (int count = 0; count < (read_count - 1); count++)
	{
		int ret = exteeprom.readSlot(
			(int)p_header->slot * SLOTNUM_MOTION_FULL() + count,
			filler + ExternalEEPROM::SLOT_SIZE() * count,
			ExternalEEPROM::SLOT_SIZE()
		);

		if (ret != ExternalEEPROM::SLOT_SIZE())
		{
			#if _DEBUG
				system.outputSerial().print(F(">>> failed : ret["));
				system.outputSerial().print(count);
				system.outputSerial().print(F("] = "));
				system.outputSerial().println(ret);
			#endif

			return false;
		}
	}

	int ret = exteeprom.readSlot(
		(int)p_header->slot * SLOTNUM_MOTION_FULL() + (read_count - 1),
		filler + ExternalEEPROM::SLOT_SIZE() * (read_count - 1),
		read_size_sup
	);

	if (ret != read_size_sup)
	{
		#if _DEBUG
			system.outputSerial().print(F(">>> failed : ret["));
			system.outputSerial().print(read_count - 1);
			system.outputSerial().print(F("] = "));
			system.outputSerial().println(ret);
		#endif

		return false;
	}

	return true;
}


bool PLEN2::MotionController::setFrame(unsigned char slot, const Frame* p_frame)
{
	#if _DEBUG
		system.outputSerial().println(F("=== in fuction : MotionController::setFrame()"));
	#endif
	
	if (slot >= Header::SLOT_END())
	{
		#if _DEBUG
			system.outputSerial().print(F(">>> bad argment : slot = "));
			system.outputSerial().println((int)slot);
		#endif

		return false;
	}

	if (p_frame->index >= Frame::FRAME_END())
	{
		#if _DEBUG
			system.outputSerial().print(F(">>> bad argment : p_frame->index = "));
			system.outputSerial().println((int)p_frame->index);
		#endif

		return false;
	}

	int frame_id = p_frame->index;
	int write_count = SLOTNUM<Frame>();
	int write_size_sup = (SIZE_SUP<Frame>()? SIZE_SUP<Frame>() : ExternalEEPROM::SLOT_SIZE());

	const char* filler = (const char*)p_frame;

	for (int count = 0; count < (write_count - 1); count++)
	{
		int ret = exteeprom.writeSlot(
			(int)slot * SLOTNUM_MOTION_FULL() + SLOTNUM<Header>() + frame_id * SLOTNUM<Frame>() + count,
			filler + ExternalEEPROM::SLOT_SIZE() * count,
			ExternalEEPROM::SLOT_SIZE()
		);

		if (ret != 0)
		{
			#if _DEBUG
				system.outputSerial().print(F(">>> failed : ret["));
				system.outputSerial().print(count);
				system.outputSerial().print(F("] = "));
				system.outputSerial().println(ret);
			#endif

			return false;
		}
	}

	int ret = exteeprom.writeSlot(
		(int)slot * SLOTNUM_MOTION_FULL() + SLOTNUM<Header>() + frame_id * SLOTNUM<Frame>() + (write_count - 1),
		filler + ExternalEEPROM::SLOT_SIZE() * (write_count - 1),
		write_size_sup
	);

	if (ret != 0)
	{
		#if _DEBUG
			system.outputSerial().print(F(">>> failed : ret["));
			system.outputSerial().print(write_count - 1);
			system.outputSerial().print(F("] = "));
			system.outputSerial().println(ret);
		#endif

		return false;
	}

	return true;
}


bool PLEN2::MotionController::getFrame(unsigned char slot, Frame* p_frame)
{
	#if _DEBUG
		system.outputSerial().println(F("=== in fuction : MotionController::getFrame()"));
	#endif
	
	if (slot >= Header::SLOT_END())
	{
		#if _DEBUG
			system.outputSerial().print(F(">>> bad argment : slot = "));
			system.outputSerial().println((int)slot);
		#endif

		return false;
	}

	if (p_frame->index >= Frame::FRAME_END())
	{
		#if _DEBUG
			system.outputSerial().print(F(">>> bad argment : p_frame->index = "));
			system.outputSerial().println((int)p_frame->index);
		#endif

		return false;
	}

	int frame_id = p_frame->index;
	int read_count = SLOTNUM<Frame>();
	int read_size_sup = (SIZE_SUP<Frame>()? SIZE_SUP<Frame>() : ExternalEEPROM::SLOT_SIZE());

	char* filler = (char*)p_frame;

	for (int count = 0; count < (read_count - 1); count++)
	{
		int ret = exteeprom.readSlot(
			(int)slot * SLOTNUM_MOTION_FULL() + SLOTNUM<Header>() + frame_id * SLOTNUM<Frame>() + count,
			filler + ExternalEEPROM::SLOT_SIZE() * count,
			ExternalEEPROM::SLOT_SIZE()
		);

		if (ret != ExternalEEPROM::SLOT_SIZE())
		{
			#if _DEBUG
				system.outputSerial().print(F(">>> failed : ret["));
				system.outputSerial().print(count);
				system.outputSerial().print(F("] = "));
				system.outputSerial().println(ret);
			#endif

			return false;
		}
	}

	int ret = exteeprom.readSlot(
		(int)slot * SLOTNUM_MOTION_FULL() + SLOTNUM<Header>() + frame_id * SLOTNUM<Frame>() + (read_count - 1),
		filler + ExternalEEPROM::SLOT_SIZE() * (read_count - 1),
		read_size_sup
	);

	if (ret != read_size_sup)
	{
		#if _DEBUG
			system.outputSerial().print(F(">>> failed : ret["));
			system.outputSerial().print(read_count - 1);
			system.outputSerial().print(F("] = "));
			system.outputSerial().println(ret);
		#endif

		return false;
	}

	return true;
}


bool PLEN2::MotionController::playing()
{
	#if _DEBUG
		system.outputSerial().println(F("=== in fuction : MotionController::playing()"));
	#endif

	return _playing;
}


bool PLEN2::MotionController::frameUpdatable()
{
	#if _DEBUG
		system.outputSerial().println(F("=== in fuction : MotionController::frameUpdatable()"));
	#endif

	return _p_joint_ctrl->_1cycle_finished;
}


bool PLEN2::MotionController::frameUpdateFinished()
{
	#if _DEBUG
		system.outputSerial().println(F("=== in fuction : MotionController::frameUpdateFinished()"));
	#endif

	return (_transition_count == 0);
}


bool PLEN2::MotionController::nextFrameLoadable()
{
	#if _DEBUG
		system.outputSerial().println(F("=== in fuction : MotionController::nextFrameLoadable()"));
	#endif

	if (   (_header.use_loop != 0)
		|| (_header.use_jump != 0) )
	{
		return true;
	}

	return ((_p_frame_next->index + 1) < _header.frame_length);
}


void PLEN2::MotionController::play(unsigned char slot)
{
	#if _DEBUG
		system.outputSerial().println(F("=== in fuction : MotionController::play()"));
	#endif

	if (playing())
	{
		#if _DEBUG
			system.outputSerial().println(F(">>> error! : A motion has been playing."));
		#endif

		return;
	}

	if (slot >= Header::SLOT_END())
	{
		#if _DEBUG
			system.outputSerial().print(F(">>> bad argment : slot = "));
			system.outputSerial().println((int)slot);
		#endif

		return;
	}

	_header.slot = slot;
	getHeader(&_header);

	_p_frame_next->index = 0;
	getFrame(_header.slot, _p_frame_next);

	_transition_count = _p_frame_next->transition_time_ms / Frame::UPDATE_INTERVAL_MS();

	for (int joint_id = 0; joint_id < JointController::SUM(); joint_id++)
	{
		_now_fixed_points[joint_id] = getFixedPoint(_p_frame_now->joint_angle[joint_id]);
		
		_diff_fixed_points[joint_id] =  getFixedPoint(_p_frame_next->joint_angle[joint_id]) - _now_fixed_points[joint_id];
		_diff_fixed_points[joint_id] /= _transition_count;
	}

	_playing = true;
}


void PLEN2::MotionController::stopping()
{
	#if _DEBUG
		system.outputSerial().println(F("=== in fuction : MotionController::stopping()"));
	#endif

	if (_header.loop_count == 255)
	{
		_header.use_loop = 0;
	}

	_header.use_jump = 0;
}


void PLEN2::MotionController::stop()
{
	#if _DEBUG
		system.outputSerial().println(F("=== in fuction : MotionController::stop()"));
	#endif

	_playing = false;
	frameBuffering(); // @attension nowフレームを正しく設定するために必須！
}


void PLEN2::MotionController::frameUpdate()
{
	#if _DEBUG
		system.outputSerial().println(F("=== in fuction : MotionController::frameUpdate()"));
	#endif	

	_transition_count--;

	for (int joint_id = 0; joint_id < JointController::SUM(); joint_id++)
	{
		_now_fixed_points[joint_id] += _diff_fixed_points[joint_id];
		_p_joint_ctrl->setAngleDiff(joint_id, getUnfixedPoint(_now_fixed_points[joint_id]));
	}

	_p_joint_ctrl->_1cycle_finished = false;
}


void PLEN2::MotionController::frameBuffering()
{
	#if _DEBUG
		system.outputSerial().println(F("=== in fuction : MotionController::frameBuffering()"));
	#endif

	Frame* p_frame_temp = _p_frame_now;
	_p_frame_now  = _p_frame_next;
	_p_frame_next = p_frame_temp;
}


void PLEN2::MotionController::loadNextFrame()
{
	#if _DEBUG
		system.outputSerial().println(F("=== in fuction : MotionController::loadNextFrame()"));
	#endif

	frameBuffering();
	const unsigned char& index_now = _p_frame_now->index;

	/*!
		@note
		ビルトイン関数の処理の優先順位は"loop" > "jump"です。
	*/
	if (_header.use_loop == 1)
	{
		if (index_now == _header.loop_end)
		{
			_p_frame_next->index = _header.loop_begin;
			getFrame(_header.slot, _p_frame_next);

			if (_header.loop_count != 255)
			{
				_header.loop_count--;
			}

			if (_header.loop_count == 0)
			{
				_header.use_loop = 0;
			}
		}
		else
		{
			_p_frame_next->index = index_now + 1;
			getFrame(_header.slot, _p_frame_next);
		}

		goto update_process;
	}

	if (_header.use_jump == 1)
	{
		if (index_now == (_header.frame_length - 1))
		{
			_header.slot = _header.jump_slot;
			getHeader(&_header);

			_p_frame_next->index = 0;
			getFrame(_header.slot, _p_frame_next);
		}
		else
		{
			_p_frame_next->index = index_now + 1;
			getFrame(_header.slot, _p_frame_next);
		}

		goto update_process;
	}

	_p_frame_next->index = index_now + 1;
	getFrame(_header.slot, _p_frame_next);


update_process:
	_transition_count = _p_frame_next->transition_time_ms / Frame::UPDATE_INTERVAL_MS();

	for (int joint_id = 0; joint_id < JointController::SUM(); joint_id++)
	{
		_now_fixed_points[joint_id] = getFixedPoint(_p_frame_now->joint_angle[joint_id]);
		
		_diff_fixed_points[joint_id] = getFixedPoint(_p_frame_next->joint_angle[joint_id]) - _now_fixed_points[joint_id];
		_diff_fixed_points[joint_id] /= _transition_count;
	}
}


void PLEN2::MotionController::dump(unsigned char slot)
{
	#if _DEBUG
		system.outputSerial().println(F("=== in fuction : MotionController::dump()"));
	#endif

	if (slot >= Header::SLOT_END())
	{
		#if _DEBUG
			system.outputSerial().print(F(">>> bad argment : slot = "));
			system.outputSerial().println((int)slot);
		#endif

		return;
	}

	Header header;
	header.slot = slot;
	getHeader(&header);

	system.outputSerial().println(F("{"));

		system.outputSerial().print(F("\t\"slot\": "));
		system.outputSerial().print((int)header.slot);
		system.outputSerial().println(F(","));

		system.outputSerial().print(F("\t\"name\": \""));
		header.name[Header::NAME_LENGTH()] = '\0'; // sanity check.
		system.outputSerial().print(header.name);
		system.outputSerial().println(F("\","));

		system.outputSerial().println(F("\t\"codes\": ["));

			if (header.use_loop == 1)
			{
			system.outputSerial().println(F("\t\t\"function\": \"loop\","));
			system.outputSerial().print(F("\t\t\"arguments\": ["));

				system.outputSerial().print((int)header.loop_begin);
				system.outputSerial().print(F(", "));
				system.outputSerial().print((int)header.loop_end);
				system.outputSerial().print(F(", "));
				system.outputSerial().print((int)header.loop_count);

			system.outputSerial().println(F("]"));
			}

			if (header.use_jump == 1)
			{
			system.outputSerial().println(F("\t\t\"function\": \"jump\","));
			system.outputSerial().print(F("\t\t\"arguments\": ["));

				system.outputSerial().print((int)header.jump_slot);

			system.outputSerial().println(F("]"));
			}

		system.outputSerial().println(F("\t],"));

		system.outputSerial().println(F("\t\"frames\": ["));

		for (int frame_index = 0; frame_index < header.frame_length; frame_index++)
		{
			Frame frame;
			frame.index = frame_index;
			getFrame(header.slot, &frame);

			system.outputSerial().println(F("\t\t{"));

				system.outputSerial().print(F("\t\t\t\"transition_time_ms\": "));
				system.outputSerial().print(frame.transition_time_ms);
				system.outputSerial().println(F(","));

				system.outputSerial().println(F("\t\t\t\"outputs\": ["));

				for (int device_index = 0; device_index < JointController::SUM(); device_index++)
				{
					system.outputSerial().println(F("\t\t\t\t{"));

						system.outputSerial().print(F("\t\t\t\t\t\"device\": "));
						system.outputSerial().print(device_index);
						system.outputSerial().println(F(","));

						system.outputSerial().print(F("\t\t\t\t\t\"value\": "));
						system.outputSerial().println(frame.joint_angle[device_index]);

					if ((device_index + 1) == JointController::SUM())
					{
					system.outputSerial().println(F("\t\t\t\t}"));
					}
					else
					{
					system.outputSerial().println(F("\t\t\t\t},"));
					}
				}

				system.outputSerial().println(F("\t\t\t]"));

			if ((frame_index + 1) == header.frame_length)
			{
			system.outputSerial().println(F("\t\t}"));
			}
			else
			{
			system.outputSerial().println(F("\t\t},"));
			}
		}

		system.outputSerial().println(F("\t]"));

	system.outputSerial().println(F("}"));
}