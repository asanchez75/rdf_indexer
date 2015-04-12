<?php

/**
 * Search service class using an RDF store server.
 */
class RdfIndexerBaseService extends SearchApiAbstractService {

  public function __construct(SearchApiServer $server) {
    parent::__construct($server);
  }
  
  /*
   * Return a representation of the triplestore
   */
  public function getStore(SearchApiIndex $index) {
    return FALSE;
  }
  /*
   * Index an item to the given store
   */
  public function indexItem($item, $store, $graph = FALSE) {}
  /*
   * Delete an item from the given store
   */
  public function deleteItem($item, $store, $graph = FALSE) {}
  /*
   * Clear a graph from the given store
   */
  public function clearGraph($store, $graph = FALSE) {}
  
  /**
   * Index the specified items.
   *
   * @param SearchApiIndex $index
   *   The search index for which items should be indexed.
   * @param array $items
   *   An array of items to be indexed, keyed by their id. The values are
   *   associative arrays of the fields to be stored, where each field is an
   *   array with the following keys:
   *   - type: One of the data types recognized by the Search API, or the
   *     special type "tokens" for fulltext fields.
   *   - original_type: The original type of the property, as defined by the
   *     datasource controller for the index's item type.
   *   - value: The value to index.
   *
   *   The special field "search_api_language" contains the item's language and
   *   should always be indexed.
   *
   *   The value of fields with the "tokens" type is an array of tokens. Each
   *   token is an array containing the following keys:
   *   - value: The word that the token represents.
   *   - score: A score for the importance of that word.
   *
   * @return array
   *   An array of the ids of all items that were successfully indexed.
   *
   * @throws SearchApiException
   *   If indexing was prevented by a fundamental configuration error.
   */
  public function indexItems(SearchApiIndex $index, array $items) {
    
    $store = $this->getStore($index);

    // @todo use documents and merge them before sending to ARC2.
    // $documents = array();
    $ret = array();

    foreach ($items as $id => $item) {
      try {
        // Builds an RDF resource for the entity.
        $rdf = rdfx_get_rdf_model($index->item_type, $id);
        $this->indexItem($rdf, $store);
        $ret[] = $id;
      }
      catch (Exception $e) {
        watchdog_exception('rdf_indexer', $e, "%type while indexing @type with ID @id: !message in %function (line %line of %file).", array('@type' => $index->item_type, '@id' => $id), WATCHDOG_WARNING);
      }
    }
    return $ret;
  }
  
  /**
   * Delete items from an index on this server.
   *
   * Might be either used to delete some items (given by their ids) from a
   * specified index, or all items from that index, or all items from all
   * indexes on this server.
   *
   * @param $ids
   *   Either an array containing the ids of the items that should be deleted,
   *   or 'all' if all items should be deleted. Other formats might be
   *   recognized by implementing classes, but these are not standardized.
   * @param SearchApiIndex $index
   *   The index from which items should be deleted, or NULL if all indexes on
   *   this server should be cleared (then, $ids has to be 'all').
   */
  public function deleteItems($ids = 'all', SearchApiIndex $index = NULL) {
  
    $store = $this->getStore($index);
    try {
      // Emtpy the local store.
      if ($ids === 'all') {
        
        //this clears the graph for a given store, but there may be more than one index storing data at the graph
        //$this->clearGraph($store);
        $ids = array_keys(entity_load($index->item_type));
      }
      if (is_array($ids)) {
        // Contructs the URI of the graph for each entity ID and deletes it.
        foreach ($ids as $id) {
          $rdf = rdfx_get_rdf_model($index->item_type, $id);
          //in case there are more than one index
          foreach($rdf->index as $subject => $triples){
            $this->deleteItem($subject, $store);
          }
        }
      } else {
        throw new Exception('Could not delete the items specified because they were not in the form of an array');
      }
    }
    catch (Exception $e) {
      watchdog_exception('rdf_indexer', $e, '%type while deleting items from server @server: !message in %function (line %line of %file).', array('@server' => $this->server->name));
    }
  }

  
  /**
   * Notifies this server that it is about to be deleted from the database and
   * should therefore clean up, if appropriate.
   *
   * Note that you shouldn't call the server's save() method, or any
   * methods that might do that, from inside of this method as the server isn't
   * present in the database anymore at this point.
   *
   * By default, deletes all indexes from this server.
   */
  public function preDelete() {
    // Only react on real deletes, not on reverts.
    // @see https://drupal.org/node/1414078
    // This method could be removed once the above issue is fixed.
    if ($this->server->hasStatus(ENTITY_IN_CODE)) {
      return;
    }
    $indexes = search_api_index_load_multiple(FALSE, array('server' => $this->server->machine_name));
    foreach ($indexes as $index) {
      $this->removeIndex($index);
    }
  }
  
  /**
   * Add a new index to this server.
   *
   * @param SearchApiIndex $index
   *   The index to add.
   */
  public function addIndex(SearchApiIndex $index) {
    if (module_exists('search_api_multi') && module_exists('search_api_views')) {
      views_invalidate_cache();
    }
  }
  
  /**
   * Notify the server that the indexed field settings for the index have
   * changed.
   * If any user action is necessary as a result of this, the method should
   * use drupal_set_message() to notify the user.
   *
   * @param SearchApiIndex $index
   *   The updated index.
   *
   * @return
   *   TRUE, if this change affected the server in any way that forces it to
   *   re-index the content. FALSE otherwise.
   */
  public function fieldsUpdated(SearchApiIndex $index) {
    if (module_exists('search_api_multi') && module_exists('search_api_views')) {
      views_invalidate_cache();
    }
    return TRUE;
  }
  
  public function search(SearchApiQueryInterface $query) {
    throw new SearchApiException(t('The RDF indexer service does not support search. Please query the SPARQL endpoint directly if the RDF store provide such service.'));
  }
}