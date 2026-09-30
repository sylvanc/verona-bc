// Fixture role: provide generic classes and nested generic scopes used by the
// module_generic_paths integration test.
// Failure mode: the parent test cannot distinguish direct, imported, aliased,
// and nested generic lookup if these definitions stop carrying their binders.
// Assumptions: this file is loaded as the `provider` sibling module.

item[T]
{
  value: T;

  create(value: T): item[T]
  {
    new {value}
  }

  get(self: item[T]): T
  {
    self.value
  }
}

outer[A]
{
  inner[B]
  {
    choose(a: A, b: B): A
    {
      a
    }
  }
}
