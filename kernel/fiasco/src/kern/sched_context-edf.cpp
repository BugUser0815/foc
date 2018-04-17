INTERFACE [sched_edf]:

#include "ready_queue_edf.h"
#include <cxx/dlist>
#include "member_offs.h"
#include "types.h"
#include "globals.h"

class Sched_context : public cxx::D_list_item //public Sched_context_edf<Sched_context>
{
	MEMBER_OFFSET();
	friend class Jdb_list_timeouts;
	friend class Jdb_thread_list;
	friend class Ready_queue_edf<Sched_context>;

	template<typename T>
	friend class Jdb_thread_list_policy;

	 union Sp
	 {
	    L4_sched_param p;
	    L4_sched_param_deadline deadline;
	 };


public:
	typedef Sched_context Edf_sc;

	 typedef cxx::Sd_list<Sched_context> Edf_list;

	 class Ready_queue_base : public Ready_queue_edf<Sched_context>
	  {
	  public:
		Ready_queue_edf<Sched_context> edf_rq;

		Sched_context *current_sched() const { return _current_sched; }
	    void activate(Sched_context *s)
	    { _current_sched = s; }


	    void ready_enqueue(Sched_context *sc)
	    {
	      //assert_kdb(cpu_lock.test());

	      // Don't enqueue threads which are already enqueued
	      if (EXPECT_FALSE (sc->in_ready_list()))
	        return;

	      enqueue(sc, (sc == current_sched()));
	    }

	  private:
	    Sched_context *_current_sched;
	  };

	 Context *context() const { return context_of(this); }


private:

	static Sched_context *edf_elem(Sched_context *x) { return x; }

	// CRUCIAL: _ready_link must have the same memory location as _ready_next from Fp_sc for in_ready_list()
	Sched_context **_ready_link;
	bool _idle:1;
	unsigned short _p;
	unsigned short _dl;
  	Unsigned64 _q;
  	Unsigned64 _left;

};

IMPLEMENTATION [sched_edf]:

#include <cassert>
#include "cpu_lock.h"
#include "kdb_ke.h"
#include "std_macros.h"
#include "config.h"

#include "kobject_dbg.h"

PUBLIC
Sched_context::Sched_context()
: _p(Config::Default_prio),
  _dl(Config::Default_deadline),
  _q(Config::Default_time_slice),
  _left(Config::Default_time_slice)
{
	//dbgprintf("[Sched_context] Created Sched_context object with type:Deadline\n");
	_ready_link = 0;
}

PUBLIC inline
unsigned short
Sched_context::prio() const
{
  return _p;
}

PUBLIC inline
unsigned
Sched_context::deadline() const
{
  return _dl;
}

PUBLIC
int
Sched_context::set(L4_sched_param const *_p)
{
	Sp const *p = reinterpret_cast<Sp const *>(_p);

	if (p->deadline.deadline == 0)
		return -L4_err::EInval;

	//dbgprintf("[Sched_context::set] Set type to Deadline (id:%lx, dl:%ld)\n",
	//		 Kobject_dbg::obj_to_id(this->context()),
	//		 p->deadline.deadline);
	_p = 0;
	_dl = p->deadline.deadline;
	_q = Config::Default_time_slice;



	return 0;
}

PUBLIC inline
Mword
Sched_context::in_ready_list() const
{
  return _ready_link != 0;
  //return Edf_list::in_list(this);
}

PUBLIC inline
bool
Sched_context::dominates(Sched_context *sc)
{
	return deadline() < sc->deadline();
}

PUBLIC inline
void
Sched_context::replenish()
{ 
  //dbgprintf("edf replenish\n");
  set_left(_q); 
}

PUBLIC inline
void
Sched_context::set_left(Unsigned64 left)
{
  //dbgprintf("edf set left\n");
  _left = left;
}


/**
 * Return remaining time quantum of Sched_context
 */
PUBLIC inline
Unsigned64
Sched_context::left() const
{
  //dbgprintf("edf left\n");
  return _left;
}

