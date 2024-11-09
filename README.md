# Enhancing MySQL: Performance, Stability, and High Availability

To better serve MySQL users, we have optimized MySQL 8.0.40 comprehensively. The specific optimizations include improvements to InnoDB storage engine scalability, redo log optimization, resolution of the join performance degradation issue since version 8.0.28, resolution of performance degradation issues in certain scenarios related to query execution plans, binlog group commit optimization, memory usage optimization, high availability enhancements, and secondary replay architecture optimization. These improvements are based on data structures, algorithms, and logical reasoning, aiming for simplicity and efficiency while streamlining the MySQL optimization process. 

Extensive testing has shown that these optimizations are especially effective on high-performance hardware. The optimized MySQL 8.0 delivers more stable and efficient service to users. 

## Our Improvements

- **Resolution of Performance Degradation Issues Since Version 8.0.28:** Join performance restored to previous levels.
- **InnoDB Storage Engine Enhancements:** Improved scalability and redo log performance in specific scenarios.
- **Performance Optimization for Binlog Group Commits.**
- **Resolution of Performance Degradation Issues in Query Execution Plans.**
- **Replica Replay Optimization:** Accelerated replay speed on replicas to ensure consistent reads.
- **Addressing Performance Issues Due to Poor MySQL NUMA Compatibility.**
- **Mature High Availability Product:** Improved Paxos protocol and protocol interactions, along with better design and enhanced cluster write performance.

These improvements are designed to enhance MySQL’s performance in high-throughput environments, achieve consistent reads, and address various failure issues, ensuring high availability and performance in high-concurrency scenarios, making it particularly suitable for internet companies.

The compiled version utilizing PGO optimization is recommended. Users interested in conducting comparative tests are encouraged to do so on high-spec machines, utilizing BenchmarkSQL for testing.

## Note

- The better the hardware environment, the greater the performance gap with the official version as concurrency increases.
- Use testing tools that closely resemble the online environment, such as BenchmarkSQL, to effectively showcase performance advantages. If possible, utilize [TCPCopy](https://github.com/session-replay-tools/tcpcopy) to replicate online traffic for testing.
- During testing, the concurrency limit should not exceed 1000, as the current throttling mechanism has not been open-sourced.
- It is recommended to align MySQL configuration parameters with our settings, making adjustments based on the specific hardware.
- For high availability, we adopted the single-primary mode of Group Replication but removed the conflict detection part, making it a fully state machine-based approach.
- Due to differences in the underlying data format of Paxos communication, it is incompatible with the official version during runtime, but compatible when offline. A restart of all nodes is required to complete the transition.
- For the improved Group Replication, we also have a highly mature middleware to provide support. For more details, refer to the project at [Cetus](https://github.com/enhancedformysql/cetus).
- For more information, refer to the main project: [enhancedformysql](https://github.com/advancedmysql/enhancedformysql).

## Download Available Releases

**The binary release version of CentOS 8.0 for x86 architecture:**

[Download Here](https://github.com/enhancedformysql/mysql-8.0.40/releases/download/mysql-8.0.40-v3.0/mysql-8.0.40-v3-for-centos8.tar.gz)  

**The binary release version of CentOS 7.0 for x86 architecture:**

[Download Here](https://github.com/enhancedformysql/mysql-8.0.40/releases/download/mysql-8.0.40-v3.0/mysql-8.0.40-v3-for-centos7.tar.gz)  

## References

For detailed principles and mechanisms behind our improvements, please refer to the following book：[The Art of Problem-Solving in Software Engineering:How to Make MySQL Better](https://enhancedformysql.github.io/The-Art-of-Problem-Solving-in-Software-Engineering_How-to-Make-MySQL-Better/)

## Bugs and Feature Requests

MySQL continues to offer numerous optimization opportunities of significant interest. If users experience any performance-related issues during actual use, [please open a new issue](https://github.com/enhancedformysql/mysql-8.0.40/issues). Before submitting a new issue, please check for any existing ones.

## Copyright and License

Copyright 2024 under [GPLv2](LICENSE).
