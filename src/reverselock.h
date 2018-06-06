// Copyright (c) 2017-2018 The Popchain Core Developers

#ifndef BITCOIN_REVERSELOCK_H
#define BITCOIN_REVERSELOCK_H

/**
 * An RAII-style reverse lock. Unlocks on construction and locks on destruction.
 */
template<typename Lock>
class reverse_lock
{
public:

    explicit reverse_lock(Lock& lock) : lock(lock) {
        lock.unlock();
		lock.swap(templock);
    }

    ~reverse_lock() {
        templock.lock();
		templock.swap(lock);
    }

private:
    reverse_lock(reverse_lock const&);
    reverse_lock& operator=(reverse_lock const&);

    Lock& lock;
	Lock templock;
};

#endif // BITCOIN_REVERSELOCK_H
