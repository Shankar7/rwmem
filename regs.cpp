#include <memory>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <inttypes.h>

#include "regs.h"
#include "helpers.h"

using namespace std;

const char* Field::name() const
{
	return m_reg.regfile().get_str(m_fd->name_offset());
}

Register::Register(const RegFile& regfile, const AddressBlockData* abd, const RegisterData* rd, const FieldData* fd)
	: m_regfile(regfile), m_abd(abd), m_rd(rd), m_fd(fd)
{
	uint32_t max = 0;
	for (unsigned i = 0; i < num_fields(); ++i) {
		uint32_t len = strlen(regfile.get_str(m_fd[i].name_offset()));
		if (len > max)
			max = len;
	}
	m_max_field_name_len = max;
}

const char*Register::name() const
{
	return m_regfile.get_str(m_rd->name_offset());
}

std::unique_ptr<Field> Register::field_by_index(unsigned idx)
{
	return std::make_unique<Field>(*this, &m_fd[idx]);
}

unique_ptr<Field> Register::find_field(const string& name)
{
	for (unsigned i = 0; i < m_rd->num_fields(); ++i) {
		if (strcmp(m_regfile.get_str(m_fd[i].name_offset()), name.c_str()) == 0)
			return make_unique<Field>(*this, &m_fd[i]);
	}

	return nullptr;
}

unique_ptr<Field> Register::find_field(uint8_t high, uint8_t low)
{
	for (unsigned i = 0; i < m_rd->num_fields(); ++i) {
		if (low == m_fd[i].low() && high == m_fd[i].high())
			return make_unique<Field>(*this, &m_fd[i]);
	}

	return nullptr;
}

RegFile::RegFile(const char* filename)
{
	int fd = open(filename, O_RDONLY);
	ERR_ON_ERRNO(fd < 0, "Open regfile '%s' failed", filename);

	off_t len = lseek(fd, (size_t)0, SEEK_END);
	lseek(fd, 0, SEEK_SET);

	void* data = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
	ERR_ON_ERRNO(data == MAP_FAILED, "mmap regfile failed");

	m_rfd = (RegFileData*)data;
	m_size = len;
}

RegFile::~RegFile()
{
	munmap((void*)m_rfd, m_size);
}

unique_ptr<Register> RegFile::find_reg(const std::string& name) const
{
	const AddressBlockData* abd = m_rfd->blocks();
	const RegisterData* rd = m_rfd->registers();
	const FieldData* fd = m_rfd->fields();

	for (unsigned bidx = 0; bidx < m_rfd->num_blocks(); ++bidx) {
		for (unsigned ridx = 0; ridx < abd->num_regs(); ++ridx) {
			if (strcmp(get_str(rd->name_offset()), name.c_str()) == 0)
				return make_unique<Register>(*this, abd, rd, fd);

			fd += rd->num_fields();
			rd++;
		}
		abd++;
	}

	return nullptr;
}

unique_ptr<Register> RegFile::find_reg(uint64_t offset) const
{
	const AddressBlockData* abd = m_rfd->blocks();
	const RegisterData* rd = m_rfd->registers();
	const FieldData* fd = m_rfd->fields();

	for (unsigned bidx = 0; bidx < m_rfd->num_blocks(); ++bidx) {
		for (unsigned ridx = 0; ridx < abd->num_regs(); ++ridx) {
			if (rd->offset() == offset)
				return make_unique<Register>(*this, abd, rd, fd);

			fd += rd->num_fields();
			rd++;
		}
		abd++;
	}

	return nullptr;
}

static void print_regfile(const RegFileData* rfd, const char* strings)
{
	printf("%s: total %u/%u/%u\n", strings + rfd->name_offset(), rfd->num_blocks(), rfd->num_regs(), rfd->num_fields());
}

static void print_address_block(const AddressBlockData* abd, const char* strings)
{
	printf("  %s: %#" PRIx64 " %#" PRIx64 ", regs %u\n", strings + abd->name_offset(), abd->offset(), abd->size(), abd->num_regs());
}

static void print_register(const RegisterData* rd, const char* strings)
{
	printf("    %s: %#" PRIx64 " %#x, fields %u\n", strings + rd->name_offset(), rd->offset(), rd->size(), rd->num_fields());
}

static void print_field(const FieldData* fd, const char* strings)
{
	printf("      %s: %u:%u\n", strings + fd->name_offset(), fd->high(), fd->low());
}

static void print_all(const RegFileData* rfd)
{
	const AddressBlockData* abd = rfd->blocks();
	const RegisterData* rd = rfd->registers();
	const FieldData* fd = rfd->fields();
	const char* strings = rfd->strings();

	print_regfile(rfd, strings);

	for (unsigned bidx = 0; bidx < rfd->num_blocks(); ++bidx, abd++) {
		print_address_block(abd, strings);

		for (unsigned ridx = 0; ridx < abd->num_regs(); ++ridx, rd++) {
			print_register(rd, strings);

			for (unsigned fidx = 0; fidx < rd->num_fields(); ++fidx, fd++) {
				print_field(fd, strings);
			}
		}
	}
}


void RegFile::print(const char* pattern)
{
	if (!pattern) {
		print_all(m_rfd);
		return;
	}

	const AddressBlockData* abd = m_rfd->blocks();
	const RegisterData* rd = m_rfd->registers();
	const FieldData* fd = m_rfd->fields();
	const char* strings = m_rfd->strings();

	bool regfile_printed = false;

	for (unsigned bidx = 0; bidx < m_rfd->num_blocks(); ++bidx, abd++) {
		bool block_printed = false;

		for (unsigned ridx = 0; ridx < abd->num_regs(); ++ridx, rd++) {
			if (strcasestr(get_str(rd->name_offset()), pattern) == NULL)
				continue;

			if (!regfile_printed) {
				print_regfile(m_rfd, strings);
				regfile_printed = true;
			}

			if (!block_printed) {
				print_address_block(abd, strings);
				block_printed = true;
			}

			print_register(rd, strings);

			for (unsigned fidx = 0; fidx < rd->num_fields(); ++fidx, fd++) {
				print_field(fd, strings);
			}
		}
	}
}
