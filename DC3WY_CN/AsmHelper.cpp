#include "AsmHelper.h"


AsmHelper::AsmElement::AsmElement(int v) {
	vtype = v;
}

int AsmHelper::AsmElement::GetLength() {
	switch (vtype)
	{
	case TYPE_SET_CMD:
		return cmdList.size();
	case TYPE_SET_VAL:
	case TYPE_GET_VAL:
		return cmdList.size() + 4;
	case TYPE_SET_JMP:
		return 5;
	default:
		break;
	}
	return 0;
}

//应用
void AsmHelper::AsmElement::Compile(uint8_t *&ptr) {
	int ii;
	switch (vtype)
	{
	case TYPE_SET_CMD:
		for (ii = 0; ii < cmdList.size(); ii++, ptr++) *ptr = cmdList[ii] ;
		break;
	case TYPE_SET_VAL:  //set跟get似乎是一致的
	case TYPE_GET_VAL:
		for (ii = 0; ii < cmdList.size(); ii++, ptr++) *ptr = cmdList[ii];
		//变量位置
		*(int*)ptr = valAddress;
		ptr += 4;
		break;
	case TYPE_SET_JMP:
		//printf("cmdList size:%d\n", cmdList.size());
		*ptr = cmdList[0]; 
		ptr++;
		*(int*)ptr = jmpAddress - (intptr_t)ptr - 4; //ptr的位置已经+1了，所以是-4
		ptr += 4;
		break;
	default:
		break;
	}
}
//==================================================

AsmHelper::AsmHelper() {
	data = nullptr;
	dataLen = 0;
	hasChage = true;
}


AsmHelper::~AsmHelper() {
	for (int ii = 0; ii < elmList.size(); ii++) delete elmList[ii];
	elmList.clear();
}


void AsmHelper::add_pushad(u8vec next) {
	auto e = elmList.emplace_back(new AsmElement(AsmElement::TYPE_SET_CMD));
	e->cmdList.push_back(0x60); //pushad
	e->cmdList.insert(e->cmdList.end(), next.begin(), next.end());
	hasChage = true;
}


void AsmHelper::add_popad(u8vec next) {
	auto e = elmList.emplace_back(new AsmElement(AsmElement::TYPE_SET_CMD));
	e->cmdList.push_back(0x61); //popad
	e->cmdList.insert(e->cmdList.end(), next.begin(), next.end());
	hasChage = true;
}


void AsmHelper::add_cmd(u8vec cmds) {
	auto e = elmList.emplace_back(new AsmElement(AsmElement::TYPE_SET_CMD));
	e->cmdList = cmds;
	hasChage = true;
}

void AsmHelper::add_valset(u8vec cmds, void* address) {
	auto e = elmList.emplace_back(new AsmElement(AsmElement::TYPE_SET_VAL));
	e->cmdList = cmds;
	e->valAddress = (intptr_t)address;
	hasChage = true;
}

void AsmHelper::add_jmp(uint8_t cmd, void* address) {
	auto e = elmList.emplace_back(new AsmElement(AsmElement::TYPE_SET_JMP));
	e->cmdList.push_back(cmd);
	//printf("add_jmp cmdlist size is:%d\n", e->cmdList.size());
	e->jmpAddress = (intptr_t)address;
	hasChage = true;
}


uint8_t* AsmHelper::GetData() {
	if (hasChage) {  //已经被改变，需要重新生成
		dataLen = CaleLength();
		if (data) delete [] data;
		data = nullptr;

		if (dataLen > 0) {
			//data不会被主动删除，故意设置成这样的
			data = new uint8_t[dataLen];
			memset(data, 0xcc, dataLen);   //设置为0xcc 可以暴露一些潜在的错误
			auto ptr = data;
			for (int ii = 0; ii < elmList.size(); ii++) {
				//printf("%d\n", ii);
				elmList[ii]->Compile(ptr);
			}
		}

		//设置权限
		DWORD OldPro = NULL;
		VirtualProtect(data, dataLen, PAGE_EXECUTE_READWRITE, &OldPro);
	}
	return data;
}

bool AsmHelper::HookForMe(uint8_t cmd, intptr_t hookAddress) {
	if(!GetData()) return false;
	uint8_t jmpmem[] = { cmd, 0,0,0,0 };
	*(int*)(jmpmem + 1) = (intptr_t)data - hookAddress - 5;
	//hook
	DWORD OldPro = NULL; SIZE_T wirteBytes = NULL;
	if (VirtualProtect((void*)hookAddress, 5, PAGE_EXECUTE_READWRITE, &OldPro)) {
		WriteProcessMemory(INVALID_HANDLE_VALUE, (void*)hookAddress, jmpmem, 5, &wirteBytes);
		//恢复
		VirtualProtect((void*)hookAddress, 5, OldPro, &OldPro);
		return true;
	}
	return false;
}

int AsmHelper::CaleLength() {
	int len = 0;
	for (auto iter = elmList.begin(); iter != elmList.end(); iter++) {
		len += (*iter)->GetLength();
	}
	return len;
}


void AsmHelper::Clear() {
	if (data) delete[] data;
	data = nullptr;
	dataLen = 0;
	elmList.clear();
}