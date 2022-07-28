#pragma once

class callable_abstract
{
public:
	virtual ~callable_abstract() = default;
	virtual void operator()() = 0;
};

template <class ReturnType>
class callable_derived final : public callable_abstract
{
public:
	void operator()()override;

	explicit callable_derived(std::packaged_task<ReturnType()>&& task);
private:
	std::packaged_task<ReturnType()> packaged_task_;
};

template <class ReturnType>
void callable_derived<ReturnType>::operator()()
{
	packaged_task_();
}

template <class ReturnType>
callable_derived<ReturnType>::callable_derived(std::packaged_task<ReturnType()>&& task) : packaged_task_(std::move(task))
{}
