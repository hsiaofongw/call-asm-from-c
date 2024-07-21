

#ifndef MY_CONN_MANAGER
#define MY_CONN_MANAGER

typedef void *conn_manage_ctx;

// 新建一个连接管理上下文
conn_manage_ctx cm_ctx_create();

// 销毁一个连接管理上下文
void cm_ctx_free(conn_manage_ctx cm_ctx, void (*before_conn_remove)(int fd));

// 记录一个连接到连接管理上下文中
void cm_ctx_add_conn(conn_manage_ctx cm_ctx, int fd);

// 遍历连接管理上下文
void cm_ctx_traverse(conn_manage_ctx cm_ctx, void *closure,
                     void (*cb)(int fd, int idx, void *closure));

// 获取连接数
int cm_ctx_get_num_conns(conn_manage_ctx cm_ctx);

// 获取最大连接 fd
int cm_ctx_get_max_fd(conn_manage_ctx cm_ctx);

// 标记已关闭连接
void cm_ctx_conn_mark_dead(conn_manage_ctx cm_ctx, int fd);

// 清除所有已关闭连接
void cm_ctx_gc(conn_manage_ctx cm_ctx, void (*before_conn_remove)(int fd));

#endif
