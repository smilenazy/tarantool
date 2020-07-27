* **Status**: In progress
* **Start date**: 27-07-2020
* **Authors**: Sergey Ostanevich @sergos \<sergos@tarantool.org\>
* **Issues**: https://github.com/tarantool/tarantool/issues/5202

## Summary

The #4842 is introduced a quorum based replication (qsync) in Tarantool environment. To augment the synchronous replication with automated leader election and failover, we need to make a choice on how to implement one of algorithms available. Our preference is Raft since it is has good comprehensibility. I would refer to the https://raft.github.io/raft.pdf further in the document.

The biggest problem is to link together the master-master nature of log replication in Tarantool with the strong leader concept in Raft. 

## Background and motivation

Customers requested synchronous replication with automated failover as part of Tarantool development. These features also fits well with Tarantool future we envision in MRG.

## Detailed design

Qsync implementation is performed in a way that allows users to run Tarantool-based solution without any changes to their code base. Literally if you have a database set up, it will continue running after upgrade the same way as it was prior to 2.5 version. You can start employing the synchronous replication by introduction of specific spaces in your schema and after that qsync with come to play. There were no changes to the protocol, so you can mix 2.5 instances with previous in both ways - replicate from new to old either vice-versa - until you introduce the first synchronous space. 

The machinery under the hood oblige all instances to follow a new process of transaction: if transaction touches a synchronous space then it will require a special command from the WAL - confirm. Since the obligation is an incremental requirement we can keep the backward compatibility and in both ways.

I expect to elaborate similar approach to the Raft-based failover machinery. Which means one can use the qsync replication without the Raft enabled, being able to elaborate its own failover mechanism. Although, if Raft is enabled then all instances in the cluster are obliged to follow the rules implied by the Raft, such as ignore log entries from a leader with stale term number.

### Log Replication

The qsync RFC explains how we enforce the log replication in a way it is described in clause 5.3 of the [1]: committed entry always has a commit message in the xlog. Key difference here is that log entry index comprises of two parts: the LSN and the served ID. The follower's log consistency will be achieved during a) leader election, when follower will only fote for a candidate who has VCLOCK components greater or equal to follower's and b) during the join to a new leader, when follower will have an option to drop it's waiting queue (named limbo in qsync implementation), either perform a full rejoin. The latter is painful, still is the only way to follow the current representation of xlog that contains no replay info.

## Rationale and alternatives

## References

[1] https://raft.github.io/raft.pdf
