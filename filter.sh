grep --line-buffered -ve '^\S*\(dump\|detail\|floodfill_resources\|pathfinding\|verbose\|action_registry\|core\.objects\|calculate_schedule\)\S*:' | sed 's/^\S*: //'
