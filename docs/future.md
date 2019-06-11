easy_grpc's API makes extensive use of a custom `Future<>` type. It deviates from `std::future<>` in a few ways:

1. It supports completion callbacks.
2. It support multiple values (which translate into multiple arguments to the completion callbacks)
3. If you are thinking of `std::future<void>`, you want `Future<>` instead.

## then

Invokes passed callback upon completion of the `Future<>`, and returns a Future that will be fullfilled with the function's return value.

    template<typename CbT> 
    [[nodiscard]] Future<Ts...>::Future<see_below> then(CbT cb);

    template<typename CbT, typename QueueT> 
    [[nodiscard]] Future<Ts...>::Future<see_below> then(CbT cb, QueueT& queue);

*Expects:* 

- `cb` is a callable taking N arguments that are convertible from `T...`.
- `queue`, if specified, accepts the equivalent of `push(std::function<void()>)`.

*Return Type:*

- Returns a `Future<T>` where `T` is the return type of `cb`.
- If `cb` returns `void`, the return type will be `Future<>`
- If `cb` returns a `Future<T...>`, the return type will be `Future<T...>` 

*On Future Fullfillment:* 

- invokes `cb`.
- if `cb` fullfills, the returned Future will be fullfilled
- if `cb` throws an exception, the returned Future will be failed.

*On Future Failure:* 

- `cb` is NOT invoked.
- the returned Future will be failed.

**Examples:**

    Future<int> x;

    Future<float> y = x.then([](int v)->float { 
        return x * 2.0f; 
    });

Multiple arguments:

    Future<int, int> x;

    Future<int> y = x.then([](int v1, int v2)->int { 
        return v1 + v2;
    });


## then_expect
    
    Invokes passed callback upon completion or failure of the `Future<>`, and returns a Future that will be fullfilled with the function's return value.

    template<typename CbT> 
    [[nodiscard]] Future<Ts...>::Future<see_below> then_expect(CbT cb);

*Expects:* 

- `cb` is a callable taking N arguments of type `expected<T...>`.

*Return Type:*

- same as `then()`

*On Future Completion or failure:* 

- invokes `cb` with expected containing either the value or the error.
- if `cb` fullfills, the returned Future will be fullfilled
- if `cb` throws an exception, the returned Future will be failed.

## then_finally_expect

    template<typename CbT> 
    void Future<Ts...>::finally(CbT cb);

The same as `then_expect()`, but the return value of the callback is ignored, and not subsequent future is generated.

## then_finally

    template<typename CbT> 
    void Future<Ts...>::then_finally(CbT cb);

The same as `then()`, but the return value of the callback is ignored, and not subsequent future is generated.

**Caveat**: If the Future is errored, even partially, then nothing will get invoked.
