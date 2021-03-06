#include "mappedregs.h"
#include "helpers.h"

using namespace std;

MappedRegisterBlock::MappedRegisterBlock(const string& mapfile, const string& regfile, const string& blockname)
{
	m_rf = make_unique<RegisterFile>(regfile);

	m_rbd = m_rf->data()->find_block(blockname);
	if (!m_rbd)
		throw runtime_error("register block not found");

	m_map = make_unique<MMapTarget>(mapfile, Endianness::Default, m_rbd->offset(), m_rbd->size());
}

MappedRegisterBlock::MappedRegisterBlock(const string& mapfile, uint64_t offset, const string& regfile, const string& blockname)
{
	m_rf = make_unique<RegisterFile>(regfile);

	m_rbd = m_rf->data()->find_block(blockname);
	if (!m_rbd)
		throw runtime_error("register block not found");

	m_map = make_unique<MMapTarget>(mapfile, Endianness::Default, offset, m_rbd->size());
}

MappedRegisterBlock::MappedRegisterBlock(const string& mapfile, uint64_t offset, uint64_t length)
	: m_rf(nullptr), m_rbd(nullptr)
{
	m_map = make_unique<MMapTarget>(mapfile, Endianness::Default, offset, length);
}

uint64_t MappedRegisterBlock::read(const string& regname) const
{
	if (!m_rf)
		throw runtime_error("no register file");

	const RegisterData* rd = m_rbd->find_register(m_rf->data(), regname);
	if (!rd)
		throw runtime_error("register not found");

	return m_map->read(rd->offset(), rd->size());
}

RegisterValue MappedRegisterBlock::read_value(const std::string& regname) const
{
	if (!m_rf)
		throw runtime_error("no register file");

	const RegisterData* rd = m_rbd->find_register(m_rf->data(), regname);
	if (!rd)
		throw runtime_error("register not found");

	uint64_t v = m_map->read(rd->offset(), rd->size());

	return RegisterValue(this, rd, v);
}

uint32_t MappedRegisterBlock::read32(uint64_t offset) const
{
	return m_map->read32(offset);
}

MappedRegister MappedRegisterBlock::find_register(const string& regname)
{
	if (!m_rf)
		throw runtime_error("no register file");

	const RegisterData* rd = m_rbd->find_register(m_rf->data(), regname);

	return MappedRegister(this, rd);
}

MappedRegister MappedRegisterBlock::get_register(uint64_t offset, uint32_t size)
{
	return MappedRegister(this, offset, size);
}


MappedRegister::MappedRegister(MappedRegisterBlock* mrb, const RegisterData* rd)
	: m_mrb(mrb), m_rd(rd), m_offset(rd->offset()), m_size(rd->size())
{

}

MappedRegister::MappedRegister(MappedRegisterBlock* mrb, uint64_t offset, uint32_t size)
	: m_mrb(mrb), m_rd(nullptr), m_offset(offset), m_size(size)
{

}

uint64_t MappedRegister::read() const
{
	return m_mrb->m_map->read(m_offset, m_size);
}

RegisterValue MappedRegister::read_value() const
{
	return RegisterValue(m_mrb, m_rd, read());
}

void MappedRegister::write(uint64_t value)
{
	m_mrb->m_map->write(m_offset, m_size, value);
}

RegisterValue::RegisterValue(const MappedRegisterBlock* mrb, const RegisterData* rd, uint64_t value)
	:m_mrb(mrb), m_rd(rd), m_value(value)
{

}

uint64_t RegisterValue::field_value(const string& fieldname) const
{
	if (!m_mrb->m_rf)
		throw runtime_error("no register file");

	const FieldData* fd = m_rd->find_field(m_mrb->m_rf->data(), fieldname);

	if (!fd)
		throw runtime_error("field not found");

	return field_value(fd->high(), fd->low());
}

uint64_t RegisterValue::field_value(uint8_t high, uint8_t low) const
{
	uint64_t mask = GENMASK(high, low);

	return (m_value & mask) >> low;
}

void RegisterValue::set_field_value(const std::string& fieldname, uint64_t value)
{
	if (!m_mrb->m_rf)
		throw runtime_error("no register file");

	const FieldData* fd = m_rd->find_field(m_mrb->m_rf->data(), fieldname);
	if (!fd)
		throw runtime_error("field not found");

	set_field_value(fd->high(), fd->low(), value);
}

void RegisterValue::set_field_value(uint8_t high, uint8_t low, uint64_t value)
{
	uint64_t mask = GENMASK(high, low);

	m_value = (m_value & ~mask) | ((value << low) & mask);
}

void RegisterValue::write()
{
	m_mrb->m_map->write(m_rd->offset(), m_rd->size(), m_value);
}
