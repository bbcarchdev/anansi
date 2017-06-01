# libcrawl

## Introduction

libcrawl provides a simple callback-driven framework for creating a web
crawler.

It doesn't itself implement queuing, caching or policies, but
provides hooks and defines interfaces for an application
(or higher-level library) to implement them, along with a
small amount of glue logic.
