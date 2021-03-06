/*
 *  fs/signalfd.c
 *
 *  Copyright (C) 2003  Linus Torvalds
 *
 *  Mon Mar 5, 2007: Davide Libenzi <davidel@xmailserver.org>
 *      Changed ->read() to return a siginfo strcture instead of signal number.
 *      Fixed locking in ->poll().
 *      Added sighand-detach notification.
 *      Added fd re-use in sys_signalfd() syscall.
 *      Now using anonymous inode source.
 *      Thanks to Oleg Nesterov for useful code review and suggestions.
 *      More comments and suggestions from Arnd Bergmann.
 *  Sat May 19, 2007: Davi E. M. Arnaut <davi@haxent.com.br>
 *      Retrieve multiple signals with one read() call
 *  Sun Jul 15, 2007: Davide Libenzi <davidel@xmailserver.org>
 *      Attach to the sighand only during read() and poll().
 */

#include <linux/file.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/list.h>
#include <linux/anon_inodes.h>
#include <linux/signalfd.h>
#include <linux/syscalls.h>
#include <linux/proc_fs.h>
#include <linux/compat.h>

void signalfd_cleanup(struct sighand_struct *sighand)
{
	wait_queue_head_t *wqh = &sighand->signalfd_wqh;
	/*
	 * The lockless check can race with remove_wait_queue() in progress,
	 * but in this case its caller should run under rcu_read_lock() and
	 * sighand_cachep is SLAB_DESTROY_BY_RCU, we can safely return.
	 */
	if (likely(!waitqueue_active(wqh)))
		return;

	/* wait_queue_t->func(POLLFREE) should do remove_wait_queue() */
	wake_up_poll(wqh, POLLHUP | POLLFREE);
}

struct signalfd_ctx {
	sigset_t sigmask;
};

static ssize_t signalfd_peek(struct signalfd_ctx *ctx,
				siginfo_t *info, loff_t *ppos)
{
	struct sigpending *pending;
	struct sigqueue *q;
	loff_t seq;
	int ret = 0;

	if (*ppos >= SFD_SHARED_QUEUE_OFFSET) {
		pending = &current->signal->shared_pending;
		seq = *ppos - SFD_SHARED_QUEUE_OFFSET;
	} else if (*ppos >= SFD_PER_THREAD_QUEUE_OFFSET) {
		pending = &current->pending;
		seq = *ppos - SFD_PER_THREAD_QUEUE_OFFSET;
	} else
		return -EINVAL;

	spin_lock_irq(&current->sighand->siglock);

	list_for_each_entry(q, &pending->list, list) {
		if (sigismember(&ctx->sigmask, q->info.si_signo))
			continue;

		if (seq-- == 0) {
			copy_siginfo(info, &q->info);
			ret = info->si_signo;
			break;
		}
	}

	spin_unlock_irq(&current->sighand->siglock);

	if (ret)
		(*ppos)++;

	return ret;
}

static int signalfd_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static unsigned int signalfd_poll(struct file *file, poll_table *wait)
{
	struct signalfd_ctx *ctx = file->private_data;
	unsigned int events = 0;

	poll_wait(file, &current->sighand->signalfd_wqh, wait);

	spin_lock_irq(&current->sighand->siglock);
	if (next_signal(&current->pending, &ctx->sigmask) ||
	    next_signal(&current->signal->shared_pending,
			&ctx->sigmask))
		events |= POLLIN;
	spin_unlock_irq(&current->sighand->siglock);

	return events;
}

/*
 * Copy a whole siginfo into userspace.
 * The main idea of this format is that it should be enough
 * for restoring siginfo back into the kernel.
 */
static int signalfd_copy_raw_info(struct signalfd_siginfo __user *siginfo,
					siginfo_t *kinfo)
{
	siginfo_t __user *uinfo = (siginfo_t __user *)siginfo;
	int err;

	BUILD_BUG_ON(sizeof(siginfo_t) != sizeof(struct signalfd_siginfo));

	err = __clear_user(uinfo, sizeof(*uinfo));

#ifdef CONFIG_COMPAT
	if (unlikely(is_compat_task())) {
		compat_siginfo_t __user *compat_uinfo;

		compat_uinfo = (compat_siginfo_t __user *)siginfo;
		err |= copy_siginfo_to_user32(compat_uinfo, kinfo);
		err |= put_user(kinfo->si_code, &compat_uinfo->si_code);

		return err ? -EFAULT : sizeof(*compat_uinfo);
	}
#endif

	err |= copy_siginfo_to_user(uinfo, kinfo);
	err |= put_user(kinfo->si_code, &uinfo->si_code);

	return err ? -EFAULT : sizeof(*uinfo);
}

/*
 * Copied from copy_siginfo_to_user() in kernel/signal.c
 */
static int signalfd_copyinfo(struct signalfd_siginfo __user *uinfo,
			     siginfo_t const *kinfo)
{
	long err;

	BUILD_BUG_ON(sizeof(struct signalfd_siginfo) != 128);

	/*
	 * Unused members should be zero ...
	 */
	err = __clear_user(uinfo, sizeof(*uinfo));

	/*
	 * If you change siginfo_t structure, please be sure
	 * this code is fixed accordingly.
	 */
	err |= __put_user(kinfo->si_signo, &uinfo->ssi_signo);
	err |= __put_user(kinfo->si_errno, &uinfo->ssi_errno);
	err |= __put_user((short) kinfo->si_code, &uinfo->ssi_code);
	switch (kinfo->si_code & __SI_MASK) {
	case __SI_KILL:
		err |= __put_user(kinfo->si_pid, &uinfo->ssi_pid);
		err |= __put_user(kinfo->si_uid, &uinfo->ssi_uid);
		break;
	case __SI_TIMER:
		 err |= __put_user(kinfo->si_tid, &uinfo->ssi_tid);
		 err |= __put_user(kinfo->si_overrun, &uinfo->ssi_overrun);
		 err |= __put_user((long) kinfo->si_ptr, &uinfo->ssi_ptr);
		 err |= __put_user(kinfo->si_int, &uinfo->ssi_int);
		break;
	case __SI_POLL:
		err |= __put_user(kinfo->si_band, &uinfo->ssi_band);
		err |= __put_user(kinfo->si_fd, &uinfo->ssi_fd);
		break;
	case __SI_FAULT:
		err |= __put_user((long) kinfo->si_addr, &uinfo->ssi_addr);
#ifdef __ARCH_SI_TRAPNO
		err |= __put_user(kinfo->si_trapno, &uinfo->ssi_trapno);
#endif
#ifdef BUS_MCEERR_AO
		/* 
		 * Other callers might not initialize the si_lsb field,
		 * so check explicitly for the right codes here.
		 */
		if (kinfo->si_code == BUS_MCEERR_AR ||
		    kinfo->si_code == BUS_MCEERR_AO)
			err |= __put_user((short) kinfo->si_addr_lsb,
					  &uinfo->ssi_addr_lsb);
#endif
		break;
	case __SI_CHLD:
		err |= __put_user(kinfo->si_pid, &uinfo->ssi_pid);
		err |= __put_user(kinfo->si_uid, &uinfo->ssi_uid);
		err |= __put_user(kinfo->si_status, &uinfo->ssi_status);
		err |= __put_user(kinfo->si_utime, &uinfo->ssi_utime);
		err |= __put_user(kinfo->si_stime, &uinfo->ssi_stime);
		break;
	case __SI_RT: /* This is not generated by the kernel as of now. */
	case __SI_MESGQ: /* But this is */
		err |= __put_user(kinfo->si_pid, &uinfo->ssi_pid);
		err |= __put_user(kinfo->si_uid, &uinfo->ssi_uid);
		err |= __put_user((long) kinfo->si_ptr, &uinfo->ssi_ptr);
		err |= __put_user(kinfo->si_int, &uinfo->ssi_int);
		break;
	default:
		/*
		 * This case catches also the signals queued by sigqueue().
		 */
		err |= __put_user(kinfo->si_pid, &uinfo->ssi_pid);
		err |= __put_user(kinfo->si_uid, &uinfo->ssi_uid);
		err |= __put_user((long) kinfo->si_ptr, &uinfo->ssi_ptr);
		err |= __put_user(kinfo->si_int, &uinfo->ssi_int);
		break;
	}

	return err ? -EFAULT: sizeof(*uinfo);
}

static ssize_t signalfd_dequeue(struct signalfd_ctx *ctx, siginfo_t *info,
				int nonblock)
{
	ssize_t ret;
	DECLARE_WAITQUEUE(wait, current);

	spin_lock_irq(&current->sighand->siglock);
	ret = dequeue_signal(current, &ctx->sigmask, info);
	switch (ret) {
	case 0:
		if (!nonblock)
			break;
		ret = -EAGAIN;
	default:
		spin_unlock_irq(&current->sighand->siglock);
		return ret;
	}

	add_wait_queue(&current->sighand->signalfd_wqh, &wait);
	for (;;) {
		set_current_state(TASK_INTERRUPTIBLE);
		ret = dequeue_signal(current, &ctx->sigmask, info);
		if (ret != 0)
			break;
		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}
		spin_unlock_irq(&current->sighand->siglock);
		schedule();
		spin_lock_irq(&current->sighand->siglock);
	}
	spin_unlock_irq(&current->sighand->siglock);

	remove_wait_queue(&current->sighand->signalfd_wqh, &wait);
	__set_current_state(TASK_RUNNING);

	return ret;
}

/*
 * Returns a multiple of the size of a "struct signalfd_siginfo", or a negative
 * error code. The "count" parameter must be at least the size of a
 * "struct signalfd_siginfo".
 */
static ssize_t signalfd_read(struct file *file, char __user *buf, size_t count,
			     loff_t *ppos)
{
	struct signalfd_ctx *ctx = file->private_data;
	struct signalfd_siginfo __user *siginfo;
	int nonblock = file->f_flags & O_NONBLOCK;
	bool raw = file->f_flags & SFD_RAW;
	ssize_t ret, total = 0;
	siginfo_t info;

	count /= sizeof(struct signalfd_siginfo);
	if (!count)
		return -EINVAL;

	siginfo = (struct signalfd_siginfo __user *) buf;
	do {
		if (*ppos == 0)
			ret = signalfd_dequeue(ctx, &info, nonblock);
		else
			ret = signalfd_peek(ctx, &info, ppos);

		if (unlikely(ret <= 0))
			break;

		if (raw)
			ret = signalfd_copy_raw_info(siginfo, &info);
		else
			ret = signalfd_copyinfo(siginfo, &info);

		if (ret < 0)
			break;
		siginfo++;
		total += ret;
		nonblock = 1;
	} while (--count);

	return total ? total: ret;
}

#ifdef CONFIG_PROC_FS
static int signalfd_show_fdinfo(struct seq_file *m, struct file *f)
{
	struct signalfd_ctx *ctx = f->private_data;
	sigset_t sigmask;

	sigmask = ctx->sigmask;
	signotset(&sigmask);
	render_sigset_t(m, "sigmask:\t", &sigmask);

	return 0;
}
#endif

static const struct file_operations signalfd_fops = {
#ifdef CONFIG_PROC_FS
	.show_fdinfo	= signalfd_show_fdinfo,
#endif
	.release	= signalfd_release,
	.poll		= signalfd_poll,
	.read		= signalfd_read,
	.llseek		= noop_llseek,
};

SYSCALL_DEFINE4(signalfd4, int, ufd, sigset_t __user *, user_mask,
		size_t, sizemask, int, flags)
{
	sigset_t sigmask;
	struct signalfd_ctx *ctx;

	/* Check the SFD_* constants for consistency.  */
	BUILD_BUG_ON(SFD_CLOEXEC != O_CLOEXEC);
	BUILD_BUG_ON(SFD_NONBLOCK != O_NONBLOCK);

	if (flags & ~(SFD_CLOEXEC | SFD_NONBLOCK | SFD_RAW))
		return -EINVAL;

	if (sizemask != sizeof(sigset_t) ||
	    copy_from_user(&sigmask, user_mask, sizeof(sigmask)))
		return -EINVAL;
	sigdelsetmask(&sigmask, sigmask(SIGKILL) | sigmask(SIGSTOP));
	signotset(&sigmask);

	if (ufd == -1) {
		struct file *file;
		ctx = kmalloc(sizeof(*ctx), GFP_KERNEL);
		if (!ctx)
			return -ENOMEM;

		ctx->sigmask = sigmask;

		ufd = get_unused_fd_flags(flags);
		if (ufd < 0) {
			kfree(ctx);
			goto out;
		}

		/*
		 * When we call this, the initialization must be complete, since
		 * anon_inode_getfd() will install the fd.
		 */
		file = anon_inode_getfile("[signalfd]", &signalfd_fops, ctx,
				       O_RDWR | (flags & (O_CLOEXEC | O_NONBLOCK)));
		if (IS_ERR(file)) {
			put_unused_fd(ufd);
			ufd = PTR_ERR(file);
			kfree(ctx);
			goto out;
		}

		file->f_flags |= flags & SFD_RAW;
		file->f_mode |= FMODE_PREAD;

		fd_install(ufd, file);
	} else {
		struct fd f = fdget(ufd);
		if (!f.file)
			return -EBADF;
		ctx = f.file->private_data;
		if (f.file->f_op != &signalfd_fops) {
			fdput(f);
			return -EINVAL;
		}
		spin_lock_irq(&current->sighand->siglock);
		ctx->sigmask = sigmask;
		spin_unlock_irq(&current->sighand->siglock);

		wake_up(&current->sighand->signalfd_wqh);
		fdput(f);
	}
out:
	return ufd;
}

SYSCALL_DEFINE3(signalfd, int, ufd, sigset_t __user *, user_mask,
		size_t, sizemask)
{
	return sys_signalfd4(ufd, user_mask, sizemask, 0);
}
