#pragma once
#ifndef __ASMHELPER_H__
#define __ASMHELPER_H__
#include <stdint.h>
#include <vector>
#include <Windows.h>
//为了更好的生成二进制文件



typedef std::vector<uint8_t> u8vec;

class AsmHelper{
public:
	class AsmElement {
	public:
		enum {
			TYPE_SET_VAL = 1,  //mov $xxx,esp
			TYPE_GET_VAL,     //mov esp,$xxx
			TYPE_SET_JMP,
			TYPE_SET_CMD,   //普通命令，直接复制即可
		};
		AsmElement(int v);
		int GetLength();
		void Compile(uint8_t *&ptr);
		int vtype;  //类型
		intptr_t valAddress;  //变量的地址
		intptr_t jmpAddress;  //跳转的地址
		u8vec cmdList;            //一系列的命令
	};


	AsmHelper();
	~AsmHelper();

	//next后面更其他指令
	void add_pushad(u8vec next);
	void add_popad(u8vec next);
	//普通添加
	void add_cmd(u8vec cmds);
	void add_valset(u8vec cmds, void* address);
	void add_jmp(uint8_t cmd, void* address);

	//获取数据
	uint8_t* GetData();
	bool HookForMe(uint8_t cmd, intptr_t hookAddress);
	//删除数据，申请的hook空间也会被删除
	void Clear();
private:
	//计算数据需要的长度
	int CaleLength();

	uint8_t *data;  //数据
	int dataLen;   //数据的长度
	bool hasChage; //是否已经改变

	//设置为指针，不然变量的拷贝烦死人
	std::vector<AsmElement*> elmList;
};

#endif // !__ASMHELPER_H__
