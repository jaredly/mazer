module type Config = {let maxHits: int; let joinOnHit: float;};

module RandomConfig = (()) => {
  let maxHits = 1 + Random.int(20);
  let joinOnHit = Random.float(1.0);
};

module type FrontierT = {
  type t('a);
  let length: t('a) => int;
  let empty: unit => t('a);
  let getAndRemoveRandom: t('a) => (option('a), t('a));
  let appendList: (t('a), list('a)) => t('a);
};

/* module FrontierArray: FrontierT = {
  type t('a) = array('a);
  let length = x => Array.length(x);
  let empty = () => [||];
  let getAndRemoveRandom = frontier =>
    switch (Js.Array.spliceInPlace(
      ~pos=Random.int(Array.length(frontier)),
      ~remove=1,
      ~add=[||],
      frontier
    )) {
      | [|item|] => (Some(item), frontier)
      | _ => (None, frontier)
    };
  let appendList = (frontier, list) => Array.append(Array.of_list(list), frontier);
}; */

module Frontier: FrontierT = {
  type t('a) = (int, list('a));
  let length = ((l, _)) => l;
  let empty = () => (0, []);
  let getAndRemoveRandom = ((length, items)) => {
    if (length == 0) {
      (None, (0, []))
    } else {
      let pos = Random.int(length);
      let rec loop = (items, n) => {
        switch (n, items) {
        | (0, [item, ...rest]) => (item, rest)
        | (_, [item, ...rest]) => {
          let (found, after) = loop(rest, n - 1);
          (found, [item, ...after])
        }
        | (_, []) => failwith("Invalid random index")
        }
      };
      let (found, after) = loop(items, pos);
      (Some(found), (length - 1, after))
    }
  };
  let appendList = ((n, items), list) => {
    (n + List.length(list), items @ list)
  };
};

module F = (Config: Config) => {
  type state = {
    visited: array(int),
    edges: Generator.PairSet.t,
    frontier: Frontier.t((int, int)),
    step: int,
    active: option(((int, int), int))
  };
  let init = (size) => {
    let start = Random.int(size);
    {
      visited: Array.make(size, 0),
      edges: Generator.PairSet.empty,
      frontier: Frontier.empty(),
      step: 0,
      active: Some(((start, start), 0))
    }
  };
  let edges = (state) => state.edges;
  let visited = (state) => state.visited;
  let max_age = (state) => state.step;
  let finished = (state) => state.active == None;
  let sortpair = (a, b) => a > b ? (b, a) : (a, b);
  let add_edge = (edges, src, dest) => Generator.PairSet.add(sortpair(src, dest), edges);
  let shouldHit = (prob) =>
    switch prob {
    | 0.0 => false
    | 1.0 => true
    | _ => Random.float(1.0) < prob
    };
  let get_new = (state) => {
    let frontier = state.frontier;
    if (Frontier.length(frontier) === 0) {
      {...state, active: None}
    } else {
      switch (Frontier.getAndRemoveRandom(frontier)) {
      | (Some(item), frontier) => {...state, frontier, active: Some((item, 0))}
      | (None, frontier) => {...state, frontier}
      }
    }
  };
  let rec step = (get_adjacent, state) => {
    let step_count = state.step;
    let {edges} = state;
    switch state.active {
    | None => state
    | Some(((src, dest), hits)) =>
      if (state.visited[dest] > 0) {
        step(get_adjacent, get_new(state))
      } else {
        let others =
          get_adjacent(dest)
          |> Utils.shuffle
          |> List.filter((x) => x !== src)
          |> List.map((x) => (dest, x));
        let step_count = step_count + 1;
        state.visited[dest] = step_count;
        let edges = add_edge(edges, src, dest);
        let rec loop = (adjacents, hit) =>
          switch adjacents {
          | [(esrc, edest), ...rest] =>
            if (state.visited[edest] > 0) {
              if (hits > Config.maxHits) {
                let edges = shouldHit(Config.joinOnHit) ? add_edge(edges, esrc, edest) : edges;
                get_new({...state, edges, step: step_count})
              } else {
                loop(rest, true)
              }
            } else {
              let frontier = Frontier.appendList(state.frontier, rest);
              {
                ...state,
                frontier,
                edges,
                step: step_count,
                active: Some(((esrc, edest), hit ? hits + 1 : hits))
              }
            }
          | _ => get_new({...state, edges, step: step_count})
          };
        loop(others, false)
      }
    }
  };
  /* let step__ = (get_adjacent, state) =>
    switch state.frontier {
    | [||] => state
    | nonEmptyArray =>
      switch (
        Js.Array.spliceInPlace(
          ~pos=Random.int(Array.length(nonEmptyArray)),
          ~remove=1,
          ~add=[||],
          nonEmptyArray
        )
      ) {
      | [|(src, dest)|] =>
        if (state.visited[dest] > 0) {
          step(get_adjacent, state)
        } else {
          state.visited[dest] = state.step + 1;
          let others =
            get_adjacent(dest)
            |> List.filter((x) => state.visited[x] === 0)
            |> List.map((x) => (dest, x));
          {
            ...state,
            step: state.step + 1,
            edges: Generator.PairSet.add(sortpair(src, dest), state.edges),
            frontier: Array.append(Array.of_list(others), nonEmptyArray)
          }
        }
      | _ => assert false
      }
    }; */
  /* hmm these can be shared */
  let rec loop_to_end = (get_adjacent, state) =>
    if (! finished(state)) {
      loop_to_end(get_adjacent, step(get_adjacent, state))
    } else {
      state
    };
  let run = (size, get_adjacent) => loop_to_end(get_adjacent, init(size));
};
