#pragma once

#ifdef TEXTURE_CACHE_DEBUG

#include "../rsx_utils.h"
#include <vector>
#include "util/vm.hpp"

namespace rsx
{

	class tex_cache_checker_t
	{
		struct per_page_info_t
		{
			u8 prot = 0;
			u8 no = 0;
			u8 ro = 0;

			FORCE_INLINE utils::protection get_protection() const
			{
				return static_cast<utils::protection>(prot);
			}

			FORCE_INLINE void set_protection(utils::protection prot)
			{
				this->prot = static_cast<u8>(prot);
			}

			FORCE_INLINE void reset_refcount()
			{
				no = 0;
				ro = 0;
			}

			FORCE_INLINE u16 sum() const
			{
				return u16{no} + ro;
			}

			FORCE_INLINE bool verify() const
			{
				const utils::protection prot = get_protection();
				switch (prot)
				{
				case utils::protection::no: return no > 0;
				case utils::protection::ro: return no == 0 && ro > 0;
				case utils::protection::rw: return no == 0 && ro == 0;
				default: fmt::throw_exception("Unreachable");
				}
			}

			FORCE_INLINE void add(utils::protection prot)
			{
				switch (prot)
				{
				case utils::protection::no:
					if (no++ == umax)
						fmt::throw_exception("add(protection::no) overflow");
					return;
				case utils::protection::ro:
					if (ro++ == umax)
						fmt::throw_exception("add(protection::ro) overflow");
					return;
				default: fmt::throw_exception("Unreachable");
				}
			}

			FORCE_INLINE void remove(utils::protection prot)
			{
				switch (prot)
				{
				case utils::protection::no:
					if (no-- == 0)
						fmt::throw_exception("remove(protection::no) overflow with NO==0");
					return;
				case utils::protection::ro:
					if (ro-- == 0)
						fmt::throw_exception("remove(protection::ro) overflow with RO==0");
					return;
				default: fmt::throw_exception("Unreachable");
				}
			}
		};
		static_assert(sizeof(per_page_info_t) <= 4, "page_info_elmnt must be less than 4-bytes in size");

		// Runtime page-size-aware storage for per-page info
		size_t page_size_;
		size_t num_pages_;
		std::vector<per_page_info_t> _info; // allocated at runtime

		tex_cache_checker_t()
		{
			page_size_ = static_cast<size_t>(utils::get_page_size());
			num_pages_ = 0x1'0000'0000u / page_size_;
			_info.resize(num_pages_);
		}

		static_assert(static_cast<u32>(utils::protection::rw) == 0, "utils::protection::rw must have value 0 for the above constructor to work");

		inline usz rsx_address_to_index(u32 address) const
		{
			return (address / static_cast<u32>(page_size_));
		}

		inline u32 index_to_rsx_address(usz idx) const
		{
			return static_cast<u32>(idx * page_size_);
		}

		inline per_page_info_t* rsx_address_to_info_pointer(u32 address)
		{
			return &(_info[rsx_address_to_index(address)]);
		}

		inline const per_page_info_t* rsx_address_to_info_pointer(u32 address) const
		{
			return &(_info[rsx_address_to_index(address)]);
		}

		inline u32 info_pointer_to_address_index(const per_page_info_t* ptr) const
		{
			return static_cast<u32>(ptr - _info.data());
		}

		inline u32 info_index_to_rsx_address(usz idx) const
		{
			return index_to_rsx_address(idx);
		}

		std::string prot_to_str(utils::protection prot) const
		{
			switch (prot)
			{
			case utils::protection::no: return "NA";
			case utils::protection::ro: return "RO";
			case utils::protection::rw: return "RW";
			default: fmt::throw_exception("Unreachable");
			}
		}

	public:
		void set_protection(const address_range& range, utils::protection prot)
		{
			AUDIT(range.is_page_range());
			AUDIT(prot == utils::protection::no || prot == utils::protection::ro || prot == utils::protection::rw);

			const usz start = rsx_address_to_index(range.start);
			const usz end = rsx_address_to_index(range.end);
			for (usz i = start; i <= end; ++i)
			{
				_info[i].set_protection(prot);
			}
		}

		void discard(const address_range& range)
		{
			set_protection(range, utils::protection::rw);
		}

		void reset_refcount()
		{
			for (usz i = 0; i < num_pages_; ++i)
			{
				_info[i].reset_refcount();
			}
		}

		void add(const address_range& range, utils::protection prot)
		{
			AUDIT(range.is_page_range());
			AUDIT(prot == utils::protection::no || prot == utils::protection::ro);

			const usz start = rsx_address_to_index(range.start);
			const usz end = rsx_address_to_index(range.end);
			for (usz i = start; i <= end; ++i)
			{
				_info[i].add(prot);
			}
		}

		void remove(const address_range& range, utils::protection prot)
		{
			AUDIT(range.is_page_range());
			AUDIT(prot == utils::protection::no || prot == utils::protection::ro);

			const usz start = rsx_address_to_index(range.start);
			const usz end = rsx_address_to_index(range.end);
			for (usz i = start; i <= end; ++i)
			{
				_info[i].remove(prot);
			}
		}

		// Returns the a lower bound as to how many locked sections are known to be within the given range with each protection {NA,RO}
		// The assumption here is that the page in the given range with the largest number of refcounted sections represents the lower bound to how many there must be
		std::pair<u8, u8> get_minimum_number_of_sections(const address_range& range) const
		{
			AUDIT(range.is_page_range());

			u8 no = 0;
			u8 ro = 0;
			const usz start = rsx_address_to_index(range.start);
			const usz end = rsx_address_to_index(range.end);
			for (usz i = start; i <= end; ++i)
			{
				no = std::max(no, _info[i].no);
				ro = std::max(ro, _info[i].ro);
			}

			return {no, ro};
		}

		void check_unprotected(const address_range& range, bool allow_ro = false, bool must_be_empty = true) const
		{
			AUDIT(range.is_page_range());
			const usz start = rsx_address_to_index(range.start);
			const usz end = rsx_address_to_index(range.end);
			for (usz i = start; i <= end; ++i)
			{
				const auto prot = _info[i].get_protection();
				if (prot != utils::protection::rw && (!allow_ro || prot != utils::protection::ro))
				{
					const u32 addr = info_index_to_rsx_address(i);
					fmt::throw_exception("Page at addr=0x%8x should be RW%s: Prot=%s, RO=%d, NA=%d", addr, allow_ro ? " or RO" : "", prot_to_str(prot), _info[i].ro, _info[i].no);
				}

				if (must_be_empty && (_info[i].no > 0 || (!allow_ro && _info[i].ro > 0)))
				{
					const u32 addr = info_index_to_rsx_address(i);
					fmt::throw_exception("Page at addr=0x%8x should not have any NA%s sections: Prot=%s, RO=%d, NA=%d", addr, allow_ro ? " or RO" : "", prot_to_str(prot), _info[i].ro, _info[i].no);
				}
			}
		}

		void verify() const
		{
			for (usz idx = 0; idx < num_pages_; idx++)
			{
				auto& elmnt = _info[idx];
				if (!elmnt.verify())
				{
					const u32 addr = info_index_to_rsx_address(idx);
					const utils::protection prot = elmnt.get_protection();
					fmt::throw_exception("Protection verification failed at addr=0x%x: Prot=%s, RO=%d, NA=%d", addr, prot_to_str(prot), elmnt.ro, elmnt.no);
				}
			}
		}
	};

	extern tex_cache_checker_t tex_cache_checker;
}; // namespace rsx
#endif // TEXTURE_CACHE_DEBUG