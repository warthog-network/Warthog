#pragma once
#include<iostream>
#include<array>

template<size_t num>
std::ostream& operator<<(std::ostream& os, const std::array<uint8_t,num>& data){
	os<<"0x";
	for (size_t i = 0; i < data.size(); ++i) {
		char map[]="0123456789abcdef";
		uint8_t d=data[i];
		os<<map[d>>4]<<map[d& 0x0Fu];
	}
	return os;
}
